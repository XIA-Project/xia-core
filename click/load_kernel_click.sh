#!/usr/bin/env bash

make -s -C `dirname $0`/ -j || exit 1

################

sudo rmmod click 2> /dev/null
sudo rmmod proclikefs 2> /dev/null

#sudo make -s -C `dirname $0`/linuxmodule/ uninstall || exit 1
#sudo make -s -C `dirname $0`/tools/click-install/ uninstall || exit 1

sudo ifdown eth2 2> /dev/null &
sudo ifdown eth3 2> /dev/null &
sudo ifdown eth4 2> /dev/null &
sudo ifdown eth5 2> /dev/null &
wait

sudo rmmod ixgbe

################

sudo modprobe ixgbe RSS=4,4,4,4

sudo ifup eth2 2> /dev/null &
sudo ifup eth3 2> /dev/null  &
sudo ifup eth4 2> /dev/null &
sudo ifup eth5 2> /dev/null  &
wait

sudo make -s -C `dirname $0`/linuxmodule/ install || exit 1
sudo make -s -C `dirname $0`/tools/click-install/ install || exit 1

################

echo
echo use: sudo click-install -t N FILENAME
echo

