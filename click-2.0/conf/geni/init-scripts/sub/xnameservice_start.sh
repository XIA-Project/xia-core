#!/bin/bash

sudo killall -9 xnameservice

cd ~/xia-core/daemons/nameserver
./xnameservice >& /dev/null &
