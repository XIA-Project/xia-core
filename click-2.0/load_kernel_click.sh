#!/usr/bin/env bash

IXGBE=`dirname $0`/../ixgbe-3.4.24


echo unloading click
sudo click-uninstall


echo stopping interfaces
(sudo ifdown eth2; sudo ifdown eth3; sudo ifdown eth4; sudo ifdown eth5; sudo ifdown eth6; sudo ifdown eth7) &


echo compiling and installing
(sudo make -j24 -s -C `dirname $0`/linuxmodule/ install-local || exit 1) &
(sudo make -j24 -s -C `dirname $0`/tools/click-install/ install-local || exit 1) &

(sudo $IXGBE/src/compile.sh install || exit 1) &


echo unloading nic module
wait %1
sudo rmmod ixgbe


echo loading nic module
wait
QCOUNT=3
sudo insmod $IXGBE/src/ixgbe.ko RSS=$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT FdirMode=0,0,0,0,0,0 DCA=0,0,0,0,0,0
#sudo insmod $IXGBE/src/ixgbe.ko RSS=$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT FdirMode=1,1,1,1,1,1 FdirQueues=$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT DCA=0,0,0,0,0,0


wait
RXQSIZE=1024
TXQSIZE=256
sudo ethtool -G eth2 rx $RXQSIZE tx $TXQSIZE &
sudo ethtool -G eth3 rx $RXQSIZE tx $TXQSIZE &
sudo ethtool -G eth4 rx $RXQSIZE tx $TXQSIZE &
sudo ethtool -G eth5 rx $RXQSIZE tx $TXQSIZE &
wait

echo use: sudo click-install -c -j NUM-THREADS CONF-FILE
echo

