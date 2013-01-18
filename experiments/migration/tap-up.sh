#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

#brctl addif br0 $1
#if [ "`hostname`" == "xia-router0" ]; then
#	ifconfig $1 hw ether 52:54:75:00:00:00
#elif [ "`hostname`" == "xia-router1" ]; then
#	ifconfig $1 hw ether 52:54:76:00:00:00
#else
#	echo not supported host
#fi
ifconfig $1 0.0.0.0 up promisc

if [ "`hostname`" == "xia-router0" ]; then
	sudo $CLICK_PATH/userlevel/click $CLICK_PATH/conf/xia/xia_vm_relay_hostA.click &
elif [ "`hostname`" == "xia-router1" ]; then
	sudo $CLICK_PATH/userlevel/click $CLICK_PATH/conf/xia/xia_vm_relay_hostB.click &
else
	echo not supported host
fi

