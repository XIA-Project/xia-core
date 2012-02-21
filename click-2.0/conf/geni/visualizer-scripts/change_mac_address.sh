#!/bin/bash

sudo ifconfig eth0 down
sudo ifconfig eth0 hw ether 00:13:72:7a:56:c9
sudo ifconfig eth0 up
sudo ifconfig eth0 | grep HWaddr

sudo ifdown eth0
sudo ifup eth0



