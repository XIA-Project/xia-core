#!/usr/bin/env bash

IXGBE=`dirname $0`/../ixgbe-3.4.24

echo compiling
make -j24 -s -C `dirname $0`/ || exit 1
pushd `dirname $0`/$IXGBE/src/
./compile.sh || exit 1
popd
sync


echo unloading click
sudo click-uninstall


echo unloading nic module
sudo ifdown eth2
sudo ifdown eth3
sudo ifdown eth4
sudo ifdown eth5

sudo rmmod ixgbe


echo loading nic module
#sudo modprobe ixgbe RSS=12,12,12,12 FdirMode=0,0,0,0
#sudo insmod $IXGBE/src/ixgbe.ko RSS=12,12,12,12 FdirMode=2,2,2,2
sudo insmod $IXGBE/src/ixgbe.ko RSS=12,12,12,12 FdirMode=0,0,0,0 #-> distributes IP packet
#sudo insmod $IXGBE/src/ixgbe.ko  FdirMode=2,2,2,2


echo installing click module
sudo make -j24 -s -C `dirname $0`/linuxmodule/ install-local || exit 1
sudo make -j24 -s -C `dirname $0`/tools/click-install/ install-local || exit 1


##echo compacting click module '(<64 MB)'
#MODPREFIX=/usr/local/lib
#sudo objcopy --only-keep-debug $MODPREFIX/click.ko $MODPREFIX/click.ko.dbg
#sudo objcopy --strip-debug $MODPREFIX/click.ko
#sudo objcopy --add-gnu-debuglink=$MODPREFIX/click.ko.dbg $MODPREFIX/click.ko


echo turning off flow control
sudo ethtool -A eth2 autoneg off rx off tx off > /dev/null
sudo ethtool -A eth3 autoneg off rx off tx off > /dev/null
sudo ethtool -A eth4 autoneg off rx off tx off > /dev/null
sudo ethtool -A eth5 autoneg off rx off tx off > /dev/null
#until sudo ethtool -A eth2 autoneg off rx off tx off > /dev/null; do echo -n .; sleep 0.1; done
#until sudo ethtool -A eth3 autoneg off rx off tx off > /dev/null; do echo -n .; sleep 0.1; done
#until sudo ethtool -A eth4 autoneg off rx off tx off > /dev/null; do echo -n .; sleep 0.1; done
#until sudo ethtool -A eth5 autoneg off rx off tx off > /dev/null; do echo -n .; sleep 0.1; done
#echo

sleep 1
sudo ifconfig eth2 promisc
sudo ifconfig eth3 promisc
sudo ifconfig eth4 promisc
sudo ifconfig eth5 promisc

echo use: sudo click-install -c -j NUM-THREADS CONF-FILE
echo

