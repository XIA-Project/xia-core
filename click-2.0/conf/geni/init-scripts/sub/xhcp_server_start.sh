#!/bin/bash

sudo killall -9 xhcp_server

cd ~/xia-core/daemons/xhcp
./xhcp_server >& /dev/null &
