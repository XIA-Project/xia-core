#!/bin/bash

make -C ../.. || exit 1

sudo opcontrol --no-vmlinux || exit 1

function run {
	sudo opcontrol --reset || exit 1
	sudo opcontrol --start || exit 1
	/usr/bin/time ./click $1.click >& output_$1_timing || exit 1
	sudo opcontrol --shutdown || exit 1
	opreport --demangle=smart --symbols click >& output_$1_oprof || exit 1
}

run ip_packetforward
run xia_packetforward_no_fallback
run xia_packetforward_fallback1
run xia_packetforward_fallback2
run xia_packetforward_update
run xia_packetforward_content_request
run xia_packetforward_content_response

