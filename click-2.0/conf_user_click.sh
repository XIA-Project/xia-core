#!/bin/sh

IO_ENGINE=`dirname $0`/../io_engine
IO_ENGINE=`readlink -f $IO_ENGINE`

cd `dirname $0`/
CPPFLAGS="$CPPFLAGS -I$IO_ENGINE/include" LDFLAGS="-L$IO_ENGINE/lib" ./configure --disable-linuxmodule --enable-warp9 --enable-user-multithread --enable-userlevel --enable-multithread=24  --enable-ip6 --enable-ipsec "$*"

make -j24 -s -C $IO_ENGINE/driver || exit 1
make -j24 -s -C $IO_ENGINE/lib || exit 1
