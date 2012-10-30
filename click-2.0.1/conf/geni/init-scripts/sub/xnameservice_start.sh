#!/bin/bash

sudo killall -9 xnameservice
sleep 1
cd ~/xia-core/daemons/nameserver
./xnameservice >& /dev/null &
