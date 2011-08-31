#!/bin/sh

cd `dirname $0`

make -j24 CFLAGS_EXTRA="-DIXGBE_NO_LRO -DIXGBE_NO_HW_RSC -g" $*
