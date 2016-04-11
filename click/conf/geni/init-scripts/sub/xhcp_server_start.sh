#!/bin/bash

sudo killall -9 xhcp_server
sleep 1
cd ~/xia-core/daemons/xhcp
./xhcp_server >& /dev/null &
