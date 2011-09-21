#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

sudo killall click; sudo $CLICK_PATH/userlevel/click $CLICK_PATH/conf/xia/xia_vm_ping_client.click

