#!/bin/bash

sudo killall -9 prep1.py
sleep 1
cd ~/xia-core/click-2.0/conf/geni/visualizer-scripts/
python ./prep1.py
