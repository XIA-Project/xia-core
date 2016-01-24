#!/bin/bash

BINPATH=../bin/

function cleanHost() {
	host=$1
	rm -f xcache.${host}
	rm -f xsockconf.ini
}

function makeHost() {
	host=$1
	cp ${BINPATH}/xcache xcache.${host}
	echo "[xcache.${host}]" >> xsockconf.ini
	echo "host=${host}" >> xsockconf.ini
	echo "" >> xsockconf.ini
}

cleanHost host0
cleanHost host1
cleanHost router0
cleanHost router1

makeHost host0
makeHost host1
makeHost router0
makeHost router1

if [ "$1" = "run" ]; then
	./xcache.host1 -h host1 -l 3 -m 0xFF&
	./xcache.host0 -h host0 -l 3 -m 0xFF&
	# ./xcache.router0 -h router0 -l 3 -m 0x0&
	# ./xcache.router1 -h router1 -l 3 -m 0x0&
	exit 0
fi

if [ "$1" = "stop" ]; then
	killall xcache.host0
	killall xcache.host1
	killall xcache.router0
	killall xcache.router1
	exit 0
fi

./xcache.${1} -h ${1} -l3 -m 0xFF
