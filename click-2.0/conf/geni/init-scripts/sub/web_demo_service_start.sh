#!/bin/bash

sudo killall -9 stock_service.py
sudo killall -9 hello_service.py
sudo killall -9 vs
sudo killall -9 webserver.py

sleep 1
cd ~/xia-core/web_demo
./stock_service.py >& /dev/null &
./hello_service.py >& /dev/null &

sleep 1
cd ~/xia-core/XIASocket/sample
./vs video.ogv >& /dev/null &

sleep 1

cd ~/xia-core/proxies
./webserver.py >& /dev/null &

