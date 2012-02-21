#!/bin/bash

sudo killall -9 stock_service.py
sudo killall -9 webserver.py
sudo killall -9 vs
sudo killall -9 hello_service.py

sudo killall -9 stock_service_replicate_geni.py
sudo killall -9 webserver_replicate_geni.py

#sleep 3
#cd ~/xia-core/web_demo
#./stock_service.py &

#sleep 4
#cd ~/xia-core/XIASocket/sample
#./vs video.ogv &

#sleep 10

#cd ~/xia-core/proxies
#./webserver.py &
