// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2016 PLUMgrid
 */
#include <linux/bpf.h>
#include <linux/if_link.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/resource.h>
#include <net/if.h>

#include "bpf_util.h"
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <getopt.h>
#include "xdp_map_access_common.h"
#include <xdp_map_access.skel.h>

struct xdp_map_access_bpf *skel = NULL;

static int ifindex;
static __u32 xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;

static void int_exit(int sig)
{
	// __u32 curr_prog_id = 0;

	// if (bpf_get_link_xdp_id(ifindex, &curr_prog_id, xdp_flags)) {
	// 	printf("bpf_get_link_xdp_id failed\n");
	// 	exit(1);
	// }
	// if (prog_id == curr_prog_id)
	// 	bpf_set_link_xdp_fd(ifindex, -1, xdp_flags);
	// else if (!curr_prog_id)
	// 	printf("couldn't find a prog id on a given interface\n");
	// else
	// 	printf("program on interface changed, not removing\n");
	xdp_map_access_bpf__destroy(skel);
	exit(0);
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
			   va_list args)
{
	return vfprintf(stderr, format, args);
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"usage: %s [OPTS] IFACE [btf]\n\n"
		"OPTS:\n"
		"    -S    use skb-mode\n"
		"    -N    enforce native mode\n"
		"    -F    force loading prog\n",
		prog);
}

int main(int argc, char **argv)
{
	// struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
	// struct bpf_prog_load_attr prog_load_attr = {
	// 	.prog_type	= BPF_PROG_TYPE_XDP,
	// };
	struct bpf_prog_info info = {};
	__u32 info_len = sizeof(info);
	const char *optstr = "FSN";
	int opt;
	// struct bpf_object *obj;
	// struct bpf_map *map;
	// char filename[256];
	int err;

	while ((opt = getopt(argc, argv, optstr)) != -1) {
		switch (opt) {
		case 'S':
			xdp_flags |= XDP_FLAGS_SKB_MODE;
			break;
		case 'N':
			/* default, set below */
			break;
		case 'F':
			xdp_flags &= ~XDP_FLAGS_UPDATE_IF_NOEXIST;
			break;
		default:
			usage(basename(argv[0]));
			return 1;
		}
	}

	if (!(xdp_flags & XDP_FLAGS_SKB_MODE))
		xdp_flags |= XDP_FLAGS_DRV_MODE;

	if (optind == argc) {
		usage(basename(argv[0]));
		return 1;
	}

	// if (setrlimit(RLIMIT_MEMLOCK, &r)) {
	// 	perror("setrlimit(RLIMIT_MEMLOCK)");
	// 	return 1;
	// }

	ifindex = if_nametoindex(argv[optind]);
	if (!ifindex) {
		perror("if_nametoindex");
		return 1;
	}
	libbpf_set_print(libbpf_print_fn);

	LIBBPF_OPTS(bpf_object_open_opts, opts, );
	if (optind + 2 == argc)
		opts.btf_custom_path = argv[optind + 1];
	// printf("optind %d %d %s\n", optind, argc, opts.btf_custom_path);

	skel = xdp_map_access_bpf__open_opts(&opts);
	if (!skel) {
		fprintf(stderr, "Failed to open BPF skeleton\n");
		return 1;
	}

	// snprintf(filename, sizeof(filename), "%s_kern.o", argv[0]);
	// prog_load_attr.file = filename;

	int res = xdp_map_access_bpf__load(skel);
	if (res) {
		fprintf(stderr, "Failed to load and verify BPF skeleton\n");
		return 1;
	}

	// attach
	res = bpf_xdp_attach(ifindex, bpf_program__fd(skel->progs.xdp_pass),
			     xdp_flags, NULL);
	if (res) {
		fprintf(stderr, "Failed to attach BPF skeleton\n");
		// return 1;
	}
	while (1)
	{
		/* code */
	}
	

	signal(SIGINT, int_exit);
	signal(SIGTERM, int_exit);

	int_exit(0);
	return 0;
}
