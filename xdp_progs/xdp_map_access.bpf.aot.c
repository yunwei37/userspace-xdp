/* Copyright (c) 2016 PLUMgrid
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 */
// #include "vmlinux.h"
// #include <bpf/bpf_helpers.h>
#include "def.bpf.h"
// #include "xdp_map_access_common.h"

// struct {
//   __uint(type, BPF_MAP_TYPE_ARRAY);
//   __type(key, __u32);
//   __type(value, __u32);
//   __uint(max_entries, CTRL_ARRAY_SIZE);
// } ctl_array SEC(".maps");

// struct {
//   __uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
//   __type(key, __u32);
//   __type(value, __u64);
//   __uint(max_entries, CNTRS_ARRAY_SIZE);
// } cntrs_array SEC(".maps");

// [2024-06-09 19:34:13.532] [info] [bpftime_shm_json.cpp:270] bpf_map_handler name=ctl_array found at 4
// [2024-06-09 19:34:13.532] [info] [bpftime_shm_json.cpp:270] bpf_map_handler name=cntrs_array found at 5
// [2024-06-09 19:34:13.532] [info] [bpftime_shm_json.cpp:265] find prog fd=6 name=xdp_pass
// INFO [1284145]: Global shm destructed

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

int bpf_main(void *ctx_base)
{
	struct xdp_md *ctx = (struct xdp_md *)ctx_base;
	void *data_end = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	__u32 ctl_flag_pos = 0;
	__u32 cntr_pos = 0;
	__u32* flag = _bpf_helper_ext_0001(((uint64_t)4 << 32), &ctl_flag_pos);

	if (!flag || (*flag != 0)) {
		return XDP_PASS;
	};

	__u64* cntr_val = _bpf_helper_ext_0001(((uint64_t)5 << 32), &cntr_pos);
	if (cntr_val) {
		*cntr_val += 1;
	};
	if (data + sizeof(struct ethhdr) > data_end)
		return XDP_DROP;
	swap_src_dst_mac(data);
	return XDP_TX;
}
