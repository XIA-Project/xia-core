#!/bin/bash

# local script 

killall -9 stock_service.py
killall -9 vs
killall -9 proxy.py 
killall -9 webserver.py

killall -9 stock_service_replicate.py
killall -9 webserver_replicate.py

killall -9 ps_beacon
killall -9 pc_beacon

killall -9 check_beacon.py

sleep 1
./stock_service.py  &
./stock_service_replicate.py &
cd ../XIASocket/sample/
./vs video.ogv &
sleep 1  # need this otherwise it doesn't work

cd ../../web
./webserver.py $@ &
./webserver_replicate.py $@  &
sleep 1
./proxy.py 15000 127.0.0.1  &

sleep 1

cd ../XIASocket/sample/
./ps_beacon &
sleep 1
./pc_beacon &

sleep 1
cd ../../web_demo/
python ./check_beacon.py &

