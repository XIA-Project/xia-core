#!/bin/sh

make -j24 CFLAGS_EXTRA="-DIXGBE_NO_LRO -DIXGBE_NO_HW_RSC -g" $*
