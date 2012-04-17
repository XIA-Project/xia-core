#!/bin/bash

sudo killall -9 xhcp_client
sleep 1
cd ~/xia-core/daemons/xhcp
./xhcp_client >& /dev/null &
