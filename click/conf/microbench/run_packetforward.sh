#!/bin/bash

function run {
	echo $1
	rm -f output_$1_timing
	rm -f output_$1_perf
	sync
	sleep 10
	./perf record -g /usr/bin/time ../../userlevel/click $1.click >& output_$1_timing
	./perf report -g flat,0 >& output_$1_perf
}

run ip_packetforward
run xia_packetforward_fallback0
run xia_packetforward_fallback1
run xia_packetforward_fallback2
run xia_packetforward_viapoint
run xia_packetforward_cid_rep

