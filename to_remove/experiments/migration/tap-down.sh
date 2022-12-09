#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh


sudo killall -q click

#brctl delif br0 $1
ifconfig $1 0.0.0.0 down

