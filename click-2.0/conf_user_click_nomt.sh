#!/bin/sh

IO_ENGINE=`dirname $0`/../io_engine
IO_ENGINE=`readlink -f $IO_ENGINE`

cd `dirname $0`/
CPPFLAGS="-I$IO_ENGINE/include" LDFLAGS="-L$IO_ENGINE/lib" ./configure --disable-linuxmodule --enable-warp9 --enable-userlevel --enable-ip6 --enable-ipsec

