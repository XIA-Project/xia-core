#!/bin/bash

sudo killall -9 xhcp_client

cd ~/xia-core/daemons/xhcp
./xhcp_client >& /dev/null &
