#!/bin/bash
make
./bin/xianet -v start
./applications/example/echoserver
trap clean_up SIGHUP SIGINT SIGTERM
function clean_up {
	echo 'stopping ...';
	./bin/xianet stop;
	exit 1
}
