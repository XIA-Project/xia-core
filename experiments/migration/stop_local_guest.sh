#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

sudo killall -q kvm
killall -q tail

