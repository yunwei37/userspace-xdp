/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/const.h>
#include <sys/resource.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include "xdp-runtime.h"
#include <net/if_arp.h>

typedef __u16 __bitwise __le16;
typedef __u16 __bitwise __be16;
typedef __u32 __bitwise __le32;
typedef __u32 __bitwise __be32;
typedef __u64 __bitwise __le64;
typedef __u64 __bitwise __be64;

typedef __u16 __bitwise __sum16;
typedef __u32 __bitwise __wsum;
typedef __s64 s64;

#define MAX_BPF_STACK 512
#define ALIGN(x, a) __ALIGN_KERNEL((x), (a))
#define ETH_HLEN 14
#define ETH_OVERHEAD (ETH_HLEN + 8 + 8)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define SKB_DATA_ALIGN(X) ALIGN(X, SMP_CACHE_BYTES)

#ifndef L1_CACHE_ALIGN
#define L1_CACHE_ALIGN(x) __ALIGN_KERNEL(x, L1_CACHE_BYTES)
#endif

#define L1_CACHE_SHIFT 5
#define L1_CACHE_BYTES (1 << L1_CACHE_SHIFT)

#ifndef SMP_CACHE_BYTES
#define SMP_CACHE_BYTES L1_CACHE_BYTES
#endif

struct bpf_scratchpad {
	union {
		__be32 diff[MAX_BPF_STACK / sizeof(__be32)];
		__u8 buff[MAX_BPF_STACK];
	};
};

static inline unsigned short from32to16(unsigned int x)
{
	/* add up 16-bit and 16-bit for 16+c bit */
	x = (x & 0xffff) + (x >> 16);
	/* add up carry.. */
	x = (x & 0xffff) + (x >> 16);
	return x;
}

#define USING_OLD_CSUM

#ifdef USING_OLD_CSUM
static unsigned int do_csum(const unsigned char *buff, int len)
{
	int odd;
	unsigned int result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long)buff;
	if (odd) {
		// #ifdef __LITTLE_ENDIAN
		result += (*buff << 8);
		// #else
		// 		result = *buff;
		// #endif
		len--;
		buff++;
	}
	if (len >= 2) {
		if (2 & (unsigned long)buff) {
			result += *(unsigned short *)buff;
			len -= 2;
			buff += 2;
		}
		if (len >= 4) {
			const unsigned char *end = buff + ((unsigned)len & ~3);
			unsigned int carry = 0;
			do {
				unsigned int w = *(unsigned int *)buff;
				buff += 4;
				result += carry;
				result += w;
				carry = (w > result);
			} while (buff < end);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *)buff;
			buff += 2;
		}
	}
	if (len & 1)
		// #ifdef __LITTLE_ENDIAN
		result += *buff;
	// #else
	// 		result += (*buff << 8);
	// #endif
	result = from32to16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}

static __wsum csum_partial(const void *buff, int len, __wsum wsum)
{
	unsigned int sum = (unsigned int)wsum;
	unsigned int result = do_csum((const unsigned char *)buff, len);

	/* add in old sum, and carry.. */
	result += sum;
	if (sum > result)
		result += 1;
	return (__wsum)result;
}

#else
/**
 * ror64 - rotate a 64-bit value right
 * @word: value to rotate
 * @shift: bits to roll
 */
static inline __u64 ror64(__u64 word, unsigned int shift)
{
	return (word >> (shift & 63)) | (word << ((-shift) & 63));
}

static inline __wsum csum_finalize_sum(uint64_t temp64)
{
	return (__wsum)((temp64 + ror64(temp64, 32)) >> 32);
}

static inline unsigned long update_csum_40b(unsigned long sum, const unsigned long m[5])
{
	asm("addq %1,%0\n\t"
	     "adcq %2,%0\n\t"
	     "adcq %3,%0\n\t"
	     "adcq %4,%0\n\t"
	     "adcq %5,%0\n\t"
	     "adcq $0,%0"
		:"+r" (sum)
		:"m" (m[0]), "m" (m[1]), "m" (m[2]),
		 "m" (m[3]), "m" (m[4]));
	return sum;
}

/*
 * Load an unaligned word from kernel space.
 *
 * In the (very unlikely) case of the word being a page-crosser
 * and the next page not being mapped, take the exception and
 * return zeroes in the non-existing part.
 */
static inline unsigned long load_unaligned_zeropad(const void *addr)
{
	return *(const unsigned long *)addr;
}

/*
 * Do a checksum on an arbitrary memory area.
 * Returns a 32bit checksum.
 *
 * This isn't as time critical as it used to be because many NICs
 * do hardware checksumming these days.
 *
 * Still, with CHECKSUM_COMPLETE this is called to compute
 * checksums on IPv6 headers (40 bytes) and other small parts.
 * it's best to have buff aligned on a 64-bit boundary
 */
__wsum csum_partial(const void *buff, int len, __wsum sum)
{
	uint64_t temp64 = (uint64_t)sum;

	/* Do two 40-byte chunks in parallel to get better ILP */
	if (likely(len >= 80)) {
		uint64_t temp64_2 = 0;
		do {
			temp64 = update_csum_40b(temp64, buff);
			temp64_2 = update_csum_40b(temp64_2, buff + 40);
			buff += 80;
			len -= 80;
		} while (len >= 80);

		asm("addq %1,%0\n\t"
		    "adcq $0,%0"
		    :"+r" (temp64): "r" (temp64_2));
	}

	/*
	 * len == 40 is the hot case due to IPv6 headers, so return
	 * early for that exact case without checking the tail bytes.
	 */
	if (len >= 40) {
		temp64 = update_csum_40b(temp64, buff);
		len -= 40;
		if (!len)
			return csum_finalize_sum(temp64);
		buff += 40;
	}

	if (len & 32) {
		asm("addq 0*8(%[src]),%[res]\n\t"
		    "adcq 1*8(%[src]),%[res]\n\t"
		    "adcq 2*8(%[src]),%[res]\n\t"
		    "adcq 3*8(%[src]),%[res]\n\t"
		    "adcq $0,%[res]"
		    : [res] "+r"(temp64)
		    : [src] "r"(buff), "m"(*(const char(*)[32])buff));
		buff += 32;
	}
	if (len & 16) {
		asm("addq 0*8(%[src]),%[res]\n\t"
		    "adcq 1*8(%[src]),%[res]\n\t"
		    "adcq $0,%[res]"
		    : [res] "+r"(temp64)
		    : [src] "r"(buff), "m"(*(const char(*)[16])buff));
		buff += 16;
	}
	if (len & 8) {
		asm("addq 0*8(%[src]),%[res]\n\t"
		    "adcq $0,%[res]"
		    : [res] "+r"(temp64)
		    : [src] "r"(buff), "m"(*(const char(*)[8])buff));
		buff += 8;
	}
	if (len & 7) {
		unsigned int shift = (-len << 3) & 63;
		unsigned long trail;

		trail = (load_unaligned_zeropad(buff) << shift) >> shift;

		asm("addq %[trail],%[res]\n\t"
		    "adcq $0,%[res]"
		    : [res] "+r"(temp64)
		    : [trail] "r"(trail));
	}
	return csum_finalize_sum(temp64);
}
#endif

uint64_t bpftime_csum_diff_runtime(uint64_t r1, uint64_t from_size, uint64_t r3,
			   uint64_t to_size, uint64_t seed)
{
	struct bpf_scratchpad sp_data;
	struct bpf_scratchpad *sp = &sp_data;
	uint64_t diff_size = from_size + to_size;
	__be32 *from = (__be32 *)(long)r1;
	__be32 *to = (__be32 *)(long)r3;
	int i, j = 0;

	/* This is quite flexible, some examples:
	 *
	 * from_size == 0, to_size > 0,  seed := csum --> pushing data
	 * from_size > 0,  to_size == 0, seed := csum --> pulling data
	 * from_size > 0,  to_size > 0,  seed := 0    --> diffing data
	 *
	 * Even for diffing, from_size and to_size don't need to be equal.
	 */
	if ((((from_size | to_size) & (sizeof(__be32) - 1)) ||
	     diff_size > sizeof(sp->diff)))
		return -EINVAL;

	for (i = 0; i < from_size / sizeof(__be32); i++, j++)
		sp->diff[j] = ~from[i];
	for (i = 0; i < to_size / sizeof(__be32); i++, j++)
		sp->diff[j] = to[i];

	return csum_partial(sp->diff, diff_size, seed);
}

struct xdp_buff {
	void *data;
	void *data_end;
	void *data_meta;
	void *data_hard_start;
	struct xdp_rxq_info *rxq;
	struct xdp_txq_info *txq;
	__u32 frame_sz;
	__u32 flags;
};

enum xdp_buff_flags {
	XDP_FLAGS_HAS_FRAGS = 1,
	XDP_FLAGS_FRAGS_PF_MEMALLOC = 2,
};

typedef s64 ktime_t;

struct skb_shared_hwtstamps {
	union {
		ktime_t hwtstamp;
		void *netdev_data;
	};
};

#define min_t(type, x, y)                                                      \
	({                                                                     \
		type _x = (x);                                                 \
		type _y = (y);                                                 \
		_x < _y ? _x : _y;                                             \
	})

typedef struct {
	int counter;
} atomic_t;

typedef struct bio_vec skb_frag_t;

struct skb_shared_info {
	__u8 flags;
	__u8 meta_len;
	__u8 nr_frags;
	__u8 tx_flags;
	short unsigned int gso_size;
	short unsigned int gso_segs;
	struct sk_buff *frag_list;
	struct skb_shared_hwtstamps hwtstamps;
	unsigned int gso_type;
	__u32 tskey;
	atomic_t dataref;
	unsigned int xdp_frags_size;
	void *destructor_arg;
	// skb_frag_t frags[17];
};

// static int bpf_xdp_frags_increase_tail(struct xdp_buff *xdp, int offset)
// {
// 	struct skb_shared_info *sinfo = xdp_get_shared_info_from_buff(xdp);
// 	skb_frag_t *frag = &sinfo->frags[sinfo->nr_frags - 1];
// 	struct xdp_rxq_info *rxq = xdp->rxq;
// 	unsigned int tailroom;

// 	if (!rxq->frag_size || rxq->frag_size > xdp->frame_sz)
// 		return -EOPNOTSUPP;

// 	tailroom = rxq->frag_size - skb_frag_size(frag) - skb_frag_off(frag);
// 	if (unlikely(offset > tailroom))
// 		return -EINVAL;

// 	memset(skb_frag_address(frag) + skb_frag_size(frag), 0, offset);
// 	skb_frag_size_add(frag, offset);
// 	sinfo->xdp_frags_size += offset;
// 	if (rxq->mem.type == MEM_TYPE_XSK_BUFF_POOL)
// 		xsk_buff_get_tail(xdp)->data_end += offset;

// 	return 0;
// }

// static void bpf_xdp_shrink_data_zc(struct xdp_buff *xdp, int shrink,
// 				   struct xdp_mem_info *mem_info, bool release)
// {
// 	struct xdp_buff *zc_frag = xsk_buff_get_tail(xdp);

// 	if (release) {
// 		xsk_buff_del_tail(zc_frag);
// 		__xdp_return(NULL, mem_info, false, zc_frag);
// 	} else {
// 		zc_frag->data_end -= shrink;
// 	}
// }

// struct xdp_mem_info {
// 	__u32 type;
// 	__u32 id;
// };

// enum xdp_mem_type {
// 	MEM_TYPE_PAGE_SHARED = 0,
// 	MEM_TYPE_PAGE_ORDER0 = 1,
// 	MEM_TYPE_PAGE_POOL = 2,
// 	MEM_TYPE_XSK_BUFF_POOL = 3,
// 	MEM_TYPE_MAX = 4,
// };

// static bool bpf_xdp_shrink_data(struct xdp_buff *xdp, skb_frag_t *frag,
// 				int shrink)
// {
// 	struct xdp_mem_info *mem_info = &xdp->rxq->mem;
// 	bool release = skb_frag_size(frag) == shrink;

// 	if (mem_info->type == MEM_TYPE_XSK_BUFF_POOL) {
// 		bpf_xdp_shrink_data_zc(xdp, shrink, mem_info, release);
// 		goto out;
// 	}

// 	if (release) {
// 		struct page *page = skb_frag_page(frag);

// 		__xdp_return(page_address(page), mem_info, false, NULL);
// 	}

// out:
// 	return release;
// }

// static int bpf_xdp_frags_shrink_tail(struct xdp_buff *xdp, int offset)
// {
// 	struct skb_shared_info *sinfo = xdp_get_shared_info_from_buff(xdp);
// 	int i, n_frags_free = 0, len_free = 0;

// 	if (unlikely(offset > (int)xdp_get_buff_len(xdp) - ETH_HLEN))
// 		return -EINVAL;

// 	for (i = sinfo->nr_frags - 1; i >= 0 && offset > 0; i--) {
// 		skb_frag_t *frag = &sinfo->frags[i];
// 		int shrink = min_t(int, offset, skb_frag_size(frag));

// 		len_free += shrink;
// 		offset -= shrink;
// 		if (bpf_xdp_shrink_data(xdp, frag, shrink)) {
// 			n_frags_free++;
// 		} else {
// 			skb_frag_size_sub(frag, shrink);
// 			break;
// 		}
// 	}
// 	sinfo->nr_frags -= n_frags_free;
// 	sinfo->xdp_frags_size -= len_free;

// 	if (unlikely(!sinfo->nr_frags)) {
// 		xdp_buff_clear_frags_flag(xdp);
// 		xdp->data_end -= offset;
// 	}

// 	return 0;
// }

/* Reserve memory area at end-of data area.
 *
 * This macro reserves tailroom in the XDP buffer by limiting the
 * XDP/BPF data access to data_hard_end.  Notice same area (and size)
 * is used for XDP_PASS, when constructing the SKB via build_skb().
 */
#define xdp_data_hard_end(xdp)                                                 \
	((xdp)->data_hard_start + (xdp)->frame_sz -                            \
	 SKB_DATA_ALIGN(sizeof(struct skb_shared_info)))

uint64_t bpftime_xdp_adjust_tail_runtime(struct xdp_buff *xdp, uint64_t offset)
{
	void *data_hard_end = xdp_data_hard_end(xdp); /* use xdp->frame_sz */
	void *data_end = xdp->data_end + offset;

	// In dpdk or afxdp, we don't use frags
	// if (unlikely(xdp_buff_has_frags(xdp))) { /* non-linear xdp buff */
	// 	if (offset < 0)
	// 		return bpf_xdp_frags_shrink_tail(xdp, -offset);
	// 	printf("offset: %lu\n bpf_xdp_frags_increase_tail not supported\n", offset);
	// 	return -EINVAL;
	// 	// return bpf_xdp_frags_increase_tail(xdp, offset);
	// }

	/* Notice that xdp_data_hard_end have reserved some tailroom */
	if (unlikely(data_end > data_hard_end))
		return -EINVAL;

	if (unlikely(data_end < xdp->data + ETH_HLEN))
		return -EINVAL;

	/* Clear memory area on grow, can contain uninit kernel memory */
	if (offset > 0)
		memset(xdp->data_end, 0, offset);

	xdp->data_end = data_end;

	return 0;
}

// ignore map id
// TODO: fix the map id
redirect_call_back_func redirect_map_callback = NULL;

void register_redirect_map_callback(int map_id, redirect_call_back_func cb)
{
	redirect_map_callback = cb;
}

uint64_t bpftime_redirect_map_runtime(uint64_t map, __u64 key, __u64 flags)
{
	if (redirect_map_callback) {
		redirect_map_callback(map, key);
		return XDP_TX;
	} else {
		printf("redirect_map_callback is NULL\n");
		return XDP_DROP;
	}
}

uint64_t bpftime_xdp_adjust_head_runtime(struct xdp_md_userspace* xdp, int offset) {
	// Do nothing because we don't use xdp meta data
	printf("bpftime_xdp_adjust_head_runtime is not supported\n");
	void *data = xdp->data + offset;
	xdp->data = data;
	return 0;
}

long bpftime_xdp_load_bytes_runtime(struct xdp_md_userspace *xdp_md, __u32 offset, void *buf, __u32 len) {
	void *data = xdp_md->data + offset;
	if (data + len > xdp_md->data_end) {
		return -EINVAL;
	}
	memcpy(buf, data, len);
	return 0;
}

int bpf_get_link_xdp_id_runtime(int ifindex, __u32 *prog_id, __u32 flags)
{
	return -1;
}

int bpf_get_link_xdp_info_runtime(int ifindex, struct xdp_link_info *info,
			  size_t info_size, __u32 flags)
{
	return -1;
}
