#!/bin/sh

IO_ENGINE=`dirname $0`/../io_engine
IO_ENGINE=`readlink -f $IO_ENGINE`

cd `dirname $0`/
CPPFLAGS="$CPPFLAGS -I$IO_ENGINE/include" LDFLAGS="-L$IO_ENGINE/lib" ./configure --disable-linuxmodule --enable-warp9 --enable-userlevel --enable-ip6 --enable-ipsec "$*"

echo ===== ignore compilation errors below if you are not doing 10 Gbps forwarding speed test
make -j24 -s -C $IO_ENGINE/driver
echo ===== ignore compilation errors above if you are not doing 10 Gbps forwarding speed test
make -j24 -s -C $IO_ENGINE/lib || exit 1
