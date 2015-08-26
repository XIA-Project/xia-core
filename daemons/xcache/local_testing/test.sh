#!/bin/bash

function xcache_stop() {
	echo "Stopping xcache"
}

if [ "$1" = "stop" ]; then
	xcache_stop
	exit 0
fi
