#!/bin/bash

make -C .. clean
make -C ../.. distclean
pushd ../.. && CXXFLAGS="-g -O2 -fno-omit-frame-pointer" ./configure --enable-task-heap && popd
make -j4 -C ../.. || exit 1

function run {
	echo $1 $2
	rm -f output_$1_$2_timing
	rm -f output_$1_$2_perf
	sync
	sleep 10
	./perf record -g /usr/bin/time ../../userlevel/click CID_RT_SIZE=$2 $1.click >& output_$1_$2_timing
	./perf report -g flat,0 >& output_$1_$2_perf
}

for SIZE in 1 1000 351611 1000000 10000000 20000000 40000000; do
	run xia_tablesize_cid $SIZE
	run xia_tablesize_ad $SIZE
done


pushd ../.. && ./configure && popd

