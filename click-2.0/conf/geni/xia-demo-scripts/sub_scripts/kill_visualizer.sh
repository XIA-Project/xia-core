#!/bin/bash

sudo killall -9 updateForwardingRate.py
sudo killall -9 updateState.py
sleep 1
python ~/xia-core/click-2.0/conf/geni/visualizer-scripts/prep1.py

