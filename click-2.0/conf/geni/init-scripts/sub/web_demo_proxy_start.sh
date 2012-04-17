#!/bin/bash

export LD_LIBRARY_PATH=:/usr/local/lib:.

sudo killall -9 proxy.py

cd ~/xia-core/proxies
./proxy.py 8080  >& /dev/null &


