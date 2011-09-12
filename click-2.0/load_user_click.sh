#!/usr/bin/env bash

IO_ENGINE=`dirname $0`/../io_engine


echo unloading click
killall click


echo stopping interfaces and unloading driver
(sudo ifdown eth2; sudo ifdown eth3; sudo ifdown eth4; sudo ifdown eth5; sudo ifdown eth6; sudo ifdown eth7; sudo rmmod ixgbe) &
(sudo rmmod ps_ixgbe) &


echo compiling driver and io_engine lib
(make -j24 -s -C $IO_ENGINE/driver || exit 1)
(make -j24 -s -C $IO_ENGINE/lib || exit 1)


echo compiling click
(make -j24 -s -C `dirname $0`/userlevel/ all || exit 1)


echo loading driver
wait
QCOUNT=12
pushd $IO_ENGINE/driver
sudo ./install.py $QCOUNT $QCOUNT eth2 eth3 eth4 eth5 eth6 eth7
popd


echo use: userlevel/click -j NUM-THREADS CONF-FILE
echo

