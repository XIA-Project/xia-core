#!/bin/bash
sudo echo Testing
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:.
#firefox &
cd ../
sudo ./click-2.0/userlevel/click ./click-2.0/conf/xia_internet0.click &
cd ./proxies
sudo ifconfig fake0 mtu 65521
sleep 1
python ./proxy.py 10000 &