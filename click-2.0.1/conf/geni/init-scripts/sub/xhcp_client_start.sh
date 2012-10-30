#!/bin/bash

sudo killall -9 xhcp_client
sleep 1
cd ~/xia-core/daemons/xhcp
host_name=$(hostname -s)
default_host_name="www_h.$host_name.com.xia"
echo $default_host_name
./xhcp_client $default_host_name >& /dev/null &
