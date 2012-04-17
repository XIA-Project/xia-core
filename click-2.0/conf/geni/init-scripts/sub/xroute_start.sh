#!/bin/bash

sudo killall -9 xroute

cd ~/xia-core/daemons/xroute
./xroute >& /dev/null &
