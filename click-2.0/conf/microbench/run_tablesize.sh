#!/bin/bash

function run {
	echo $1 $2
	sync
	for ITER in 0 1 2 3 4 5 6 7 8 9; do
	#for ITER in 0 1 2 3 4; do
		echo $ITER
		if [ -e "output_$1_$2_timing_$ITER" ]; then
			echo skipping
			continue
		fi
		sleep 3
		perf record -g /usr/bin/time ../../userlevel/click CID_RT_SIZE=$2 $1.click >& output_$1_$2_timing_$ITER
		perf report -g flat,0 >& output_$1_$2_perf_$ITER
	done
}

#for SIZE in 10000 30000 100000 300000 1000000 3000000 10000000 30000000; do
for SIZE in 10000 30000 100000 300000 1000000 3000000 10000000; do
	run xia_tablesize_cid $SIZE
done

#for SIZE in 10; do
#	run xia_tablesize_ad $SIZE
#done

