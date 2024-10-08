# Userspace XDP for ebpf load balancer

Use virtual env to run the userspace XDP load balancer. for test.

- [Userspace XDP for ebpf load balancer](#userspace-xdp-for-ebpf-load-balancer)
  - [Setup testbed](#setup-testbed)
  - [default: use kernel xdp load balance](#default-use-kernel-xdp-load-balance)
  - [run with AF\_XDP and bpftime](#run-with-af_xdp-and-bpftime)
  - [run with dpdk](#run-with-dpdk)


## Setup testbed

For details on how to setup the test bed, see [venv-details.md](venv-details.md)

DPDK:

```sh
scripts/testbed-setup.sh
```

eBPF:

```sh
scripts/testbed-setup.sh ebpf
```

And also

```sh
sudo iptables -P FORWARD ACCEPT
sudo ip netns exec h2 ip link set lo up
```

The old `xdp-ebpf` has some bug in bpftime, so we use the new one in `xdp-ebpf-new`. (it's not critical, can be fix later)

## default: use kernel xdp load balance

To build the provided code run in xdp-ebpf-new directory:

```sh
make
```

To load the eBPF program run the following:

```sh
lb bash
xdp_progs/xdp-lb/xdp_lb veth6
```

Note: make sure you have run `source alias.sh` before, so that `lb` make sense.
As before, this alias allows you to execute commands in the load balancer network namespace.

To make sure that everything works well, the provided eBPF skeleton drops all incoming TCP and ICMP packets.
Trying running `ping 10.0.0.10` before and after loading the eBPF program.
When the eBPF program is loaded you should not get any ICMP responses.

To unload the eBPF program run:

```sh
lb ip link set dev veth6 xdpgeneric off
```

## run with AF_XDP and bpftime

build bpftime library

```sh
cmake -B build-bpftime .  -DBUILD_BPFTIME_DAEMON=0
make -C  build-bpftime -j
```

load the eBPF XDP program into shared memory

```sh
scripts/testbed-setup.sh ebpf
source scripts/aliases.sh
lb bash
LD_PRELOAD=build/bpftime/runtime/syscall-server/libbpftime-syscall-server.so SPDLOG_LEVEL=trace xdp_progs/xdp-lb/xdp_lb veth6 base.btf
```

problem: data in xdp_md is 32 bit, while kernel will convert it into 64 bit.

The xdp_md is:

```c
struct xdp_md {
    __u32 data;
    __u32 data_end;
    __u32 data_meta;
    __u32 ingress_ifindex;
    __u32 rx_queue_index;
    __u32 egress_ifindex;
};
```

it will cause in userspace:

```txt
received packet 0x7fea635ee100, send data to eBPF module len 42
received packet 0x635ee100 0x635ee12a
Segmentation fault (core dumped)
```

However, our jit doesn't do that.

see <https://lists.iovisor.org/g/iovisor-dev/topic/question_about_struct_xdp_md/21385959?p=>

Solution: let BTF help us.

This is kernel BTF relocation, run with the above command:

```txt
libbpf: prog 'xdp_pass': relo #0: <byte_off> [19] struct xdp_md.data (0:0 @ offset 0)
libbpf: prog 'xdp_pass': relo #0: matching candidate #0 <byte_off> [6034] struct xdp_md.data (0:0 @ offset 0)
libbpf: prog 'xdp_pass': relo #0: patched insn #0 (LDX/ST/STX) off 0 -> 0
libbpf: prog 'xdp_pass': relo #0: patched insn #0 (LDX/ST/STX) mem_sz 8 -> 4
libbpf: prog 'xdp_pass': relo #1: <byte_off> [19] struct xdp_md.data_end (0:1 @ offset 8)
libbpf: prog 'xdp_pass': relo #1: matching candidate #0 <byte_off> [6034] struct xdp_md.data_end (0:1 @ offset 4)
libbpf: prog 'xdp_pass': relo #1: patched insn #1 (LDX/ST/STX) off 8 -> 4
libbpf: prog 'xdp_pass': relo #1: patched insn #1 (LDX/ST/STX) mem_sz 8 -> 4
libbpf: CO-RE relocating [24] struct iphdr: found target candidate [11775] struct iphdr in [vmlinux]
```

This is for usespace CO-RE commands:

```txt
LD_PRELOAD=build/bpftime/runtime/syscall-server/libbpftime-syscall-server.so SPDLOG_LEVEL=trace xdp_progs/xdp-lb/xdp_lb veth6 base.btf
```

(ignore the error message)

And run the xd_xdp_user application in the testbed:

```sh
source scripts/aliases.sh
lb bash
cd afxdp/src
./af_xdp_user veth6
```

test:

In two different terminals run two netcat servers in the two namespaces

```sh
source scripts/aliases.sh
h2 bash
python3 -m http.server
```

```sh
source scripts/aliases.sh
h3 bash
python3 -m http.server
```

From your machine run to connect to one of the servers and send some messages.

```sh
curl -vv 10.0.0.10:8000
```

Then you will get the data in both terminals.

## run with dpdk

compile the dpdk-ebpf

```sh
export PKG_CONFIG_PATH=<the path of the pkgconfig directory inside dpdk>
# e.g. export PKG_CONFIG_PATH=/path/to/repo/external/dpdk/install-dir/lib/x86_64-linux-gnu/pkgconfig
make dpdk
make bench-bin
```

Set up the testbed

```sh
sudo scripts/hugepages.sh
scripts/testbed-setup.sh 
```

Run bpftime server

```sh
LD_PRELOAD=build/bpftime/runtime/syscall-server/libbpftime-syscall-server.so SPDLOG_LEVEL=trace xdp_progs/xdp-lb/xdp_lb veth6 base.btf
```

Run the dpdk server

```sh
sudo -E LD_LIBRARY_PATH=:/usr/lib64/:build/bpftime/libbpf/:afxdp/lib/xdp-tools/lib/libxdp/:/home/yunwei/ebpf-xdp-dpdk/build-bpftime-llvm/bpftime/libbpf dpdk/dpdk_llvm -l 1  --socket-mem=512 -a 0000:18:00.1 -- -p 0x1
```

Link the tap interface to the dpdk server

```sh
scripts/link-dpdk.sh
```

From your machine try `arp`ing the middlebox.

```sh
arping 10.0.0.10
```
