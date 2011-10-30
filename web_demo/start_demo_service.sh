#!/bin/bash

# local script 

killall -9 stock_service.py
killall -9 vs
killall -9 proxy.py 
killall -9 webserver.py

./stock_service.py  > /dev/null &
cd ../XIASocket/sample/
./vs video.ogv > /dev/null &
sleep 1  # need this otherwise it doesn't work

cd ../../proxies
./webserver.py $@ &
sleep 1
./proxy.py > /dev/null &
