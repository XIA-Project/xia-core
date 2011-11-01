#!/bin/bash

sudo killall -9 updateForwardingRate.py
sudo killall -9 updateState.py
sleep 1
sh ~/xia-core/click-2.0/conf/geni/stats/xia-link-state.sh | ~/vis-scripts/updateForwardingRate.py R1-H1 utah &
echo Forwarding | ~/vis-scripts/updateState.py H1 utah &

