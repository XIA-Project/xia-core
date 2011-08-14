#!/usr/bin/env bash

echo compiling
make -s -C `dirname $0`/ -j24 || exit 1
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
sudo modprobe ixgbe RSS=12,12,12,12 FdirMode=0,0,0,0


echo installing click module
sudo make -s -C `dirname $0`/linuxmodule/ install-local || exit 1
sudo make -s -C `dirname $0`/tools/click-install/ install-local || exit 1


echo compacting click module '(<64 MB)'
MODPREFIX=/usr/local/lib
sudo objcopy --only-keep-debug $MODPREFIX/click.ko $MODPREFIX/click.ko.dbg
sudo objcopy --strip-debug $MODPREFIX/click.ko
sudo objcopy --add-gnu-debuglink=$MODPREFIX/click.ko.dbg $MODPREFIX/click.ko

echo turning off flow control
until sudo ethtool -A eth2 autoneg off rx off tx off > /dev/null; do echo -n .; sleep 0.1; done
until sudo ethtool -A eth3 autoneg off rx off tx off > /dev/null; do echo -n .; sleep 0.1; done
until sudo ethtool -A eth4 autoneg off rx off tx off > /dev/null; do echo -n .; sleep 0.1; done
until sudo ethtool -A eth5 autoneg off rx off tx off > /dev/null; do echo -n .; sleep 0.1; done
echo


echo use: sudo click-install -t NUM-THREADS CONF-FILE
echo

