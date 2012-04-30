#!/bin/bash

sudo killall -9 xroute
sleep 1
cd ~/xia-core/daemons/xroute
./xroute >& /dev/null &
