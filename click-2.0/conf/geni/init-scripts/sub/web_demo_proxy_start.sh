#!/bin/bash

sudo killall -9 proxy.py

cd ~/xia-core/proxies
./proxy.py 8080  >& /dev/null &


