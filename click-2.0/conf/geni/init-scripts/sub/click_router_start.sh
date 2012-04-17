#!/bin/bash

cd ~/xia-core/click-2.0 
sudo killall -9 click 
sleep 1
sudo userlevel/click conf/geni/init-scripts/template/router.click >& /dev/null &

