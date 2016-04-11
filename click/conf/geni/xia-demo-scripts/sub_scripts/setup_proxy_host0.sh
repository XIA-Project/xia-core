#!/bin/bash

export LD_LIBRARY_PATH=:/usr/local/lib:.

cd ~/xia-core/proxies
sudo killall -9 proxy.py
sleep 1
./proxy.py 8080 >& /dev/null &
