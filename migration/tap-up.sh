#!/bin/sh

brctl addif br0 $1
ifconfig $1 0.0.0.0 up promisc

