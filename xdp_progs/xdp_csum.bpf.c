/* Copyright (c) 2016 PLUMgrid
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 */
#include "def.bpf.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define LOOP_LEN 32
#define ETH_P_IP 0x0800
#define ETH_P_IPV6 0x86DD

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__type(key, u32);
	__type(value, long);
	__uint(max_entries, 256);
} rxcnt SEC(".maps");

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

static __always_inline __u16 csum_fold_helper(__u32 csum)
{
	return ~((csum & 0xffff) + (csum >> 16));
}

static __always_inline void ipv4_csum(void *data_start, int data_size,
				      __u32 *csum)
{
	*csum = bpf_csum_diff(0, 0, data_start, data_size, *csum);
	*csum = csum_fold_helper(*csum);
}

SEC("xdp")
int xdp_pass(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct ethhdr *eth = data;
	struct iphdr *iph;
	int rc = XDP_PASS;
	long *value;
	u16 h_proto;
	u64 nh_off;
	u32 dummy_int = 23;
	__u32 csum = 0;
	int i = 0;
	// bpf_printk("received packet %p %p\n", data, data_end);
	nh_off = sizeof(*eth);
	if (data + nh_off > data_end)
		return rc;

	h_proto = eth->h_proto;
	
	if (h_proto != bpf_htons(ETH_P_IP))
		return rc;

	iph = data + nh_off;
	// bpf_printk("iph->protocol\n");
	nh_off +=sizeof(*iph);
	if (data + nh_off  > data_end)
		return rc;
	
	for (i = 0; i < LOOP_LEN ;i++){
		ipv4_csum(iph, sizeof(struct iphdr), &csum);
		iph->check = csum;
		value = bpf_map_lookup_elem(&rxcnt, &dummy_int);
	}
	// bpf_printk("csum: %d\n", csum);

	value = bpf_map_lookup_elem(&rxcnt, &dummy_int);
	if (value)
		*value += 1;
	swap_src_dst_mac(data);
	rc = XDP_TX;
	return rc;
}


char _license[] SEC("license") = "GPL";
