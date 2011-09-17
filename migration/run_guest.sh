#!/bin/bash

PREFIX="`dirname $0`"
PREFIX="`readlink -f $PREFIX`"
source $PREFIX/common.sh

killall -q kvm
killall -q tail
sleep 1
nohup kvm $COMMON_ARGS > stdout 2> stderr &
tail -f stderr &

