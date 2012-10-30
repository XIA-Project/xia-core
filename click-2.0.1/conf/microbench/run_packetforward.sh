#!/bin/bash

function run {
	echo $1
	sync
	for ITER in 0 1 2 3 4 5 6 7 8 9; do
	#for ITER in 0 1 2 3 4; do
	#for ITER in 0 1; do
		echo $ITER
		if [ -e "output_$1_timing_$ITER" ]; then
			echo skipping
			continue
		fi
		sleep 3
		perf record -g /usr/bin/time ../../userlevel/click $1.click >& output_$1_timing_$ITER
		perf report -g flat,0 >& output_$1_perf_$ITER
	done
}

run ip_packetforward
run ip_packetforward_fastpath
run xia_packetforward_fallback0
run xia_packetforward_fallback1
run xia_packetforward_fallback2
run xia_packetforward_fallback3
run xia_packetforward_viapoint
#run xia_packetforward_cid_rep
run xia_packetforward_fallback3_fastpath

