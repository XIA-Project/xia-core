#!/bin/bash

make -C .. clean
make -C ../.. distclean
pushd ../.. && CXXFLAGS="-g -O2 -fno-omit-frame-pointer" ./configure --enable-task-heap && popd
make -j4 -C ../.. || exit 1

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
run xia_packetforward_no_fallback
run xia_packetforward_fallback1
run xia_packetforward_fallback2
run xia_packetforward_update
run xia_packetforward_content_request_miss
run xia_packetforward_content_request_hit
run xia_packetforward_content_response


pushd ../.. && ./configure && popd

