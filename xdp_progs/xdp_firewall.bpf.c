/*
 * Soft:        xdp_fw stand for XDP Firewall. It offers a simple layer3
 * 		packet filtering. Operation are really fast since it
 * 		doesnt travel/traverse kernel to apply kernel rules unlike
 * 		netfilter. Initial goal for this module is to provide a packet
 * 		isolation/filtering for Keepalived VRRP framework.
 *
 * Part:        XDP eBPF source code to be loaded into kernel.
 *
 * Author:      Alexandre Cassen, <acassen@keepalived.org>
 *
 *              This program is distributed in the hope that it will be useful,
 *              but WITHOUT ANY WARRANTY; without even the implied warranty of
 *              MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *              See the GNU General Public License for more details.
 *
 *              This program is free software; you can redistribute it and/or
 *              modify it under the terms of the GNU General Public License
 *              as published by the Free Software Foundation; either version
 *              2 of the License, or (at your option) any later version.
 *
 * Copyright (C) 2019 Alexandre Cassen, <acassen@gmail.com>
 */


struct flow_key {
	union {
		unsigned int addr;
		unsigned int addr6[4];
	};
	unsigned int proto;
} __attribute__((__aligned__(8)));

struct vrrp_filter {
	unsigned int action;
	unsigned long long drop_packets;
	unsigned long long total_packets;
	unsigned long long drop_bytes;
	unsigned long long total_bytes;
} __attribute__((__aligned__(8)));

#include "def.bpf.h"
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

/* bpf_trace_printk() output:
 * /sys/kernel/debug/tracing/trace_pipe
 */
// #define bpf_printk(fmt, ...)                                                   \
// 	({                                                                     \
// 		char ____fmt[] = fmt;                                          \
// 		bpf_trace_printk(____fmt, sizeof(____fmt), ##__VA_ARGS__);     \
// 	})


struct ip_auth_hdr {
	__u8 nexthdr;
	__u8 hdrlen;
	__be16 reserved;
	__be32 spi;
	__be32 seq_no;
	__u8 auth_data[0];
};

#define ETH_ALEN 6
#define ETH_P_802_3_MIN 0x0600
#define ETH_P_8021Q 0x8100
#define ETH_P_8021AD 0x88A8
#define ETH_P_IP 0x0800
#define ETH_P_IPV6 0x86DD
#define ETH_P_ARP 0x0806
#define IPPROTO_ICMPV6 58

#define EINVAL 22
#define ENETDOWN 100
#define EMSGSIZE 90
#define EOPNOTSUPP 95
#define ENOSPC 28

/* linux/if_vlan.h have not exposed this as UAPI, thus mirror some here
 *
 *      struct vlan_hdr - vlan header
 *      @h_vlan_TCI: priority and VLAN ID
 *      @h_vlan_encapsulated_proto: packet type ID or len
 */
struct _vlan_hdr {
	__be16 hvlan_TCI;
	__be16 h_vlan_encapsulated_proto;
};

struct vrrphdr {
	__u8 vers_type;
	__u8 vrid;
	__u8 priority;
	__u8 naddr;
	union {
		struct {
			__u8 auth_type;
			__u8 adver_int;
		} v2;
		struct {
			__u16 adver_int;
		} v3;
	};
	__u16 chksum;
} __attribute__((__packed__));
#define IPPROTO_VRRP 112

#define ICMPV6_ND_NEIGHBOR_SOLICIT 135
#define ICMPV6_ND_NEIGHBOR_ADVERT 136

struct parse_pkt {
	__u16 l3_proto;
	__u16 l3_offset;
};

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__type(key, struct flow_key);
	__type(value, __u64);
	__uint(max_entries, 32768);
	__uint(map_flags, BPF_F_NO_PREALLOC);
} l3_filter SEC(".maps");

// struct bpf_map_def SEC("maps") vrrp_vrid_filter = {
// 	.type = BPF_MAP_TYPE_PERCPU_ARRAY,
// 	.key_size = sizeof (__u32),
// 	.value_size = sizeof (struct vrrp_filter),
// 	.max_entries = 256,
// 	.map_flags = 0,
// };
struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__type(key, __u32);
	__type(value, struct vrrp_filter);
	__uint(max_entries, 256);
} vrrp_vrid_filter SEC(".maps");

/* ICMPv6 filtering */
// static __always_inline bool icmp6_accept(struct icmp6hdr *icmp6h)
// {
// 	if (icmp6h->icmp6_type == ICMPV6_ND_NEIGHBOR_SOLICIT ||
// 	    icmp6h->icmp6_type == ICMPV6_ND_NEIGHBOR_ADVERT)
// 		return true;
// 	return false;
// }

/* VRRP filtering */
static __always_inline int vrrp_filter(struct vrrphdr *vrrph, int len)
{
	struct vrrp_filter *vrrpf;
	int key = vrrph->vrid;

	vrrpf = bpf_map_lookup_elem(&vrrp_vrid_filter, &key);
	if (!vrrpf)
		return XDP_PASS;

	vrrpf->total_packets++;
	vrrpf->total_bytes += len;
	if (vrrpf->action)
		return XDP_PASS;

	vrrpf->drop_packets++;
	vrrpf->drop_bytes += len;
	return XDP_DROP;
}

/* IP filtering */
static __always_inline int layer3_filter(void *data, void *data_end,
					 struct parse_pkt *pkt)
{
	struct iphdr *iph;
	struct ipv6hdr *ip6h;
	struct icmp6hdr *icmp6h;
	struct vrrphdr *vrrph = NULL;
	struct ip_auth_hdr *ah;
	struct flow_key key = {};
	int offset = 0, tot_len = 0;
	__u64 *drop_cnt;

	/* Room sanitize */
	if (pkt->l3_proto == ETH_P_IP) {
		iph = data + pkt->l3_offset;
		if (iph + 1 > data_end)
			return XDP_PASS;
		/* FIXME: fragmentation handling */
		tot_len = bpf_ntohs(iph->tot_len);
		offset += pkt->l3_offset;
		key.proto = ETH_P_IP;
		key.addr6[1] = key.addr6[2] = key.addr6[3] = 0;
		key.addr = iph->daddr;
		if (iph->protocol == IPPROTO_VRRP) {
			vrrph = data + offset + sizeof(struct iphdr);
			if (vrrph + 1 > data_end)
				return XDP_DROP;
		} else if (iph->protocol == IPPROTO_AH) {
			ah = data + offset + sizeof(struct iphdr);
			if (ah + 1 > data_end)
				return XDP_PASS;
			offset += sizeof(struct iphdr);
			if (ah->nexthdr == IPPROTO_VRRP) {
				vrrph = data + offset +
					sizeof(struct ip_auth_hdr);
				if (vrrph + 1 > data_end)
					return XDP_DROP;
			}
		}
	} else if (pkt->l3_proto == ETH_P_IPV6) {
		bpf_printk("only ipv4 is supported\n");
		// ip6h = data + pkt->l3_offset;
		// if (ip6h + 1 > data_end)
		// 	return XDP_PASS;

		// /* ICMPv6 filtering */
		// if (ip6h->nexthdr == IPPROTO_ICMPV6) {
		// 	icmp6h = data + pkt->l3_offset + sizeof(struct ipv6hdr);
		// 	if (icmp6h + 1 > data_end)
		// 		return XDP_DROP;

		// 	if (icmp6_accept(icmp6h))
		// 		return XDP_PASS;
		// }

		// /* FIXME: fragmentation handling */
		// tot_len = bpf_ntohs(ip6h->payload_len);
		// key.proto = ETH_P_IPV6;
		// __builtin_memcpy(key.addr6, ip6h->daddr.s6_addr32,
		// 		 sizeof (key.addr6));
		// if (ip6h->nexthdr == IPPROTO_VRRP) {
		// 	vrrph = data + pkt->l3_offset + sizeof(struct ipv6hdr);
		// 	if (vrrph + 1 > data_end)
		// 		return XDP_DROP;
		// }
		return XDP_DROP;
	} else {
		return XDP_PASS;
	}

	drop_cnt = bpf_map_lookup_elem(&l3_filter, &key);
	if (drop_cnt) {
		*drop_cnt += 1;
		return XDP_DROP;
	}

	if (vrrph)
		return vrrp_filter(vrrph, tot_len);

	return XDP_PASS;
}

/* Ethernet frame parsing and sanitize */
static __always_inline bool parse_eth_frame(struct ethhdr *eth, void *data_end,
					    struct parse_pkt *pkt)
{
	struct _vlan_hdr *vlan_hdr;
	__u16 eth_type;
	__u8 offset;

	offset = sizeof(*eth);

	/* Make sure packet is large enough for parsing eth */
	if ((void *)eth + offset > data_end)
		return false;

	eth_type = eth->h_proto;

	/* Handle outer VLAN tag */
	if (eth_type == bpf_htons(ETH_P_8021Q) ||
	    eth_type == bpf_htons(ETH_P_8021AD)) {
		vlan_hdr = (void *)eth + offset;
		offset += sizeof(*vlan_hdr);
		if ((void *)eth + offset > data_end)
			return false;

		eth_type = vlan_hdr->h_vlan_encapsulated_proto;
	}

	/* Handle inner (Q-in-Q) VLAN tag */
	if (eth_type == bpf_htons(ETH_P_8021Q) ||
	    eth_type == bpf_htons(ETH_P_8021AD)) {
		vlan_hdr = (void *)eth + offset;
		offset += sizeof(*vlan_hdr);
		if ((void *)eth + offset > data_end)
			return false;

		eth_type = vlan_hdr->h_vlan_encapsulated_proto;
	}

	pkt->l3_proto = bpf_ntohs(eth_type);
	pkt->l3_offset = offset;
	return true;
}


static void swap_src_dst_mac(void *data)
{
	unsigned short *p = data;
	unsigned short dst[3];

	dst[0] = p[0];
	dst[1] = p[1];
	dst[2] = p[2];
	p[0] = p[3];
	p[1] = p[4];
	p[2] = p[5];
	p[3] = dst[0];
	p[4] = dst[1];
	p[5] = dst[2];
}


SEC("xdp")
int xdp_pass(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct parse_pkt pkt = { 0 };

	if (!parse_eth_frame(data, data_end, &pkt))
		return XDP_PASS;

	int ret = layer3_filter(data, data_end, &pkt);
	if (ret == XDP_PASS) {
		// here we sent out the packet
		swap_src_dst_mac(data);
		return XDP_TX;
	}
	return ret;
}

char _license[] SEC("license") = "GPL";
