#!/bin/bash

export LD_LIBRARY_PATH=:/usr/local/lib:.

sudo killall -9 proxy.py

sleep 1
cd ~/xia-core/web
./proxy.py 8080  >& /dev/null &


