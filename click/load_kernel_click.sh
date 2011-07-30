#!/usr/bin/env bash

make -C `dirname $0`/ -j || exit 1

################

sudo rmmod click
sudo rmmod proclikefs

sudo make -C `dirname $0`/linuxmodule/ uninstall || exit 1

sudo ifdown eth2
sudo ifdown eth3
sudo ifdown eth4
sudo ifdown eth5

sudo rmmod ixgbe

################

sudo modprobe ixgbe RSS=4,4,4,4

sudo ifup eth2
sudo ifup eth3
sudo ifup eth4
sudo ifup eth5

sudo make -C `dirname $0`/linuxmodule/ install || exit 1

################

echo
echo use: sudo `dirname $0`/tools/click-install/click-install -t N FILENAME
echo

