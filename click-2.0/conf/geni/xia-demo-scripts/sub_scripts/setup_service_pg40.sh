#!/bin/bash

sudo killall -9 stock_service.py
sudo killall -9 webserver.py
sudo killall -9 vs
cd ~/xia-core/proxies
sleep 3
./webserver.py &
cd ~/xia-core/web_demo
./stock_service.py &
cd ~/xia-core/XIASocket/sample
./vs video.ogv &