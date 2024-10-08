-- CGCS Demo script
--
-- SPDX-License-Identifier: BSD-3-Clause

package.path = package.path .. ";?.lua;test/?.lua;app/?.lua;../?.lua"

require "Pktgen";

sendport = 0;
recvport = 0;
pkt_size	= 64;
local dstip = "192.168.1.13";
local srcip = "192.168.1.11";
local netmask = "/24";
local total_time = 30;

-- Stop the port sending and reset to
pktgen.stop(sendport);
sleep(2);  -- Wait for stop to happen (not really needed)

pktgen.set_ipaddr(sendport, "dst", dstip);
pktgen.set_ipaddr(sendport, "src", srcip .. netmask);
pktgen.set_mac(sendport, "dst", "b8:3f:d2:2a:e7:69")
pktgen.set_proto(sendport .. "," .. recvport, "tcp");
-- v is the table to values created by the Set(x,y) functio

pktgen.set(sendport, "size", pkt_size);

printf("   Percent 100 load  %d seconds\n", total_time);

-- Set the rate to the new value
pktgen.set(sendport, "rate", 100);

-- If not sending packets start sending them
printf("\n**** Traffic Profile start\n");


function getLatency(a)
    local port_stats = {}
    local num_lat_pkts = 0
    local min_cycles = 0
    local avg_cycles = 0
    local max_cycles = 0
    local min_us = 0
    local avg_us = 0
    local max_us = 0
    local mbits_rx = 0
    local mbits_tx = 0

    if a.pktsize == 0 then a.pktsize = default_pktsize end
    if a.sleeptime == 0 then a.sleeptime = default_sleeptime end
    if a.rate < 0 then a.rate = default_rate end

    pktgen.set(a.sendport, "count", 0)
    pktgen.set(a.sendport, "burst", 4)
    pktgen.set(a.sendport, "rate", a.rate)
    pktgen.set(a.sendport, "size", a.pktsize)

    -- enable latency
    pktgen.latency(a.sendport, "enable");
    pktgen.latency(a.sendport, "rate", 1000);
    pktgen.latency(a.sendport, "entropy", 12);
    pktgen.latency(a.recvport, "enable");
    pktgen.latency(a.recvport, "rate", 10000);
    pktgen.latency(a.recvport, "entropy", 8);

    pktgen.delay(100);

    -- Start traffic
    pktgen.start(a.sendport)
    startTime = os.time()

    for i = 1, a.iterations, 1 do
        pktgen.sleep(a.sleeptime)
        t1 = os.difftime(os.time(), startTime)

        port_stats = pktgen.pktStats(a.recvport);
        num_lat_pkts = port_stats[tonumber(a.recvport)].latency.num_pkts
        num_skipped = port_stats[tonumber(a.recvport)].latency.num_skipped
        min_cycles = port_stats[tonumber(a.recvport)].latency.min_cycles
        avg_cycles = port_stats[tonumber(a.recvport)].latency.avg_cycles
        max_cycles = port_stats[tonumber(a.recvport)].latency.max_cycles
        min_us = port_stats[tonumber(a.recvport)].latency.min_us
        avg_us = port_stats[tonumber(a.recvport)].latency.avg_us
        max_us = port_stats[tonumber(a.recvport)].latency.max_us

        mbits_rx = pktgen.portStats(a.recvport, "rate")[tonumber(a.recvport)].mbits_rx
        mbits_tx = pktgen.portStats(a.sendport, "rate")[tonumber(a.sendport)].mbits_tx

        printf("%4d %8d %14d %14d %14d %12.2f %12.2f %12.2f %6d %6d %8d\n", t1, num_lat_pkts,
            min_cycles, avg_cycles, max_cycles,
            min_us, avg_us, max_us, mbits_rx, mbits_tx, num_skipped)
    end
    pktgen.stop(a.sendport)

    pktgen.latency(a.sendport, "disable");
    pktgen.latency(a.recvport, "disable");

    return 0
end

-- pktgen.page("latency")
pktgen.screen("off")

printf("Latency Samples\n")
printf("%4s %8s %14s %14s %14s %12s %12s %12s %6s %6s %8s\n", "time", "nbPkts",
    "minCycles", "avgCycles", "maxCycles",
    "min_us", "avg_us", "max_us", "RxMB", "TxMB", "Skipped")
pktgen.sleep(2)

min_latency = getLatency{
    sendport=0,
    recvport=0,
    rate=0.00001,
    pktsize=pkt_size,
    sleeptime=total_time,
    iterations=1
}

print("Done")


printf("\n*** Traffic Profile Done (Total Time %d) ***\n", total_time);
