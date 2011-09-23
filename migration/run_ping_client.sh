#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

sudo killall click
rm -f output

tail -F output | grep "client seq = .*000," &
sudo $CLICK_PATH/userlevel/click $CLICK_PATH/conf/xia/xia_vm_ping_client.click > output 2>&1
kill %1

