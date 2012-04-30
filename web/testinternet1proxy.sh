#!/bin/bash
sudo echo Testing
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
cd ../
sudo ./click-2.0/userlevel/click ./click-2.0/conf/xia_internet1.click &
cd ./proxies
sudo ifconfig fake1 mtu 65521
sleep 1
python ./webserver.py &
