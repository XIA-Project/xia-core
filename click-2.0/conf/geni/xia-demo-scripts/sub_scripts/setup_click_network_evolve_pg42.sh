#!/bin/bash

cd ~/xia-core/click-2.0 
sudo killall -9 click 
#sudo userlevel/click conf/geni/router1-pg42.click &
sudo userlevel/click conf/geni/router1-pg42-connect-cmu.click &
