#!/bin/bash

sudo killall -9 proxy.py

cd ~/xia-core/web
./proxy.py 8080  >& /dev/null &


