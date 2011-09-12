#!/usr/bin/env bash

IXGBE=`dirname $0`/../ixgbe-3.4.24


echo unloading click
sudo click-uninstall


echo stopping interfaces and unloading driver
(sudo ifdown eth2; sudo ifdown eth3; sudo ifdown eth4; sudo ifdown eth5; sudo ifdown eth6; sudo ifdown eth7; sudo rmmod ixgbe) &


echo compiling and installing
(sudo make -j24 -s -C `dirname $0`/linuxmodule/ install-local || exit 1)
(sudo make -j24 -s -C `dirname $0`/tools/click-install/ install-local || exit 1)

(sudo $IXGBE/src/compile.sh install || exit 1)


echo loading nic module
wait
QCOUNT=12
DELAY=0
INT=956
#sudo insmod $IXGBE/src/ixgbe.ko RSS=$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT FdirMode=0,0,0,0,0,0 DCA=0,0,0,0,0,0 InterruptThrottleRate=$INT,$INT,$INT,$INT,$INT,$INT
sudo insmod $IXGBE/src/ixgbe.ko RSS=$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT FdirMode=0,0,0,0,0,0 DCA=0,0,0,0,0,0 PollDelay=$DELAY,$DELAY,$DELAY,$DELAY,$DELAY,$DELAY #InterruptThrottleRate=$INT,$INT,$INT,$INT,$INT,$INT
#sudo insmod $IXGBE/src/ixgbe.ko RSS=$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT FdirMode=1,1,1,1,1,1 FdirQueues=$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT,$QCOUNT DCA=0,0,0,0,0,0 PollDelay=$DELAY,$DELAY,$DELAY,$DELAY,$DELAY,$DELAY


wait
RXQSIZE=4048
TXQSIZE=4048
sudo ethtool -G eth2 rx $RXQSIZE tx $TXQSIZE &
sudo ethtool -G eth3 rx $RXQSIZE tx $TXQSIZE &
sudo ethtool -G eth4 rx $RXQSIZE tx $TXQSIZE &
sudo ethtool -G eth5 rx $RXQSIZE tx $TXQSIZE &
wait

sudo $IXGBE/scripts/set_irq_affinity.sh eth2 eth3 eth4 eth5

echo use: sudo click-install -c -j NUM-THREADS CONF-FILE
echo

