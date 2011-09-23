#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

$PREFIX/stop_local_guest.sh
sleep 1

nohup sudo kvm $COMMON_ARGS $INCOMING_ARGS > /dev/null &
sleep 1

