#!/bin/bash

cd ~/xia-core/click-2.0 
sudo killall -9 click 
sudo userlevel/click conf/geni/router1.click &
#sudo userlevel/click conf/geni/router1-connect-cmu.click &
