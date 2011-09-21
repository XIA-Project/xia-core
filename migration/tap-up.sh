#!/bin/bash

brctl addif br0 $1
if [ "`hostname`" == "xia-router0" ]; then
	ifconfig $1 hw ether 52:54:75:00:00:00
else
	ifconfig $1 hw ether 52:54:76:00:00:00
fi
ifconfig $1 0.0.0.0 up promisc

