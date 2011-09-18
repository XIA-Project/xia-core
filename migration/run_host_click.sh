#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

ssh $HOST_A_IP bash -c "sudo killall click; $CLICK_PATH/userlevel/click $CLICK_PATH/conf/xia/vm_hostA.conf"

ssh $HOST_B_IP bash -c "sudo killall click; $CLICK_PATH/userlevel/click $CLICK_PATH/conf/xia/vm_hostB.conf"

