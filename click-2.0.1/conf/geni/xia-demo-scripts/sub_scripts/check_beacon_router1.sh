#!/bin/bash

killall -9 check_beacon.py

cd ~/xia-core/click-2.0/conf/geni/stats

sleep 1
python ./check_beacon.py &
disown
