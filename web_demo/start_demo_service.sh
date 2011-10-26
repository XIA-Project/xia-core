#!/bin/bash
killall -9 stock_service.py
killall -9 vs
killall -9 proxy.py 
./stock_service.py &
cd ../XIASocket/sample/
./vs video.ogv &

cd ../../proxies
./proxy.py &
