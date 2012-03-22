#!/bin/bash

export LD_LIBRARY_PATH=:/usr/local/lib:.

sudo killall -9 stock_service.py
sudo killall -9 webserver.py
sudo killall -9 vs
sudo killall -9 hello_service.py

sudo killall -9 stock_service_replicate_geni.py
sudo killall -9 webserver_replicate_geni.py

sleep 1
cd ~/xia-core/web_demo
./stock_service.py >& /dev/null &
./hello_service.py >& /dev/null &
./stock_service_replicate_geni.py >& /dev/null &

sleep 1
cd ~/xia-core/XIASocket/sample
./vs video.ogv >& /dev/null &

sleep 1

cd ~/xia-core/proxies
./webserver.py >& /dev/null &
./webserver_replicate_geni.py >& /dev/null &
