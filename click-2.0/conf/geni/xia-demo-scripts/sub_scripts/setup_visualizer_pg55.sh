#!/bin/bash

sudo killall -9 updateForwardingRate.py
sudo killall -9 updateState.py
sleep 3
sh ~/xia-core/click-2.0/conf/geni/stats/xia-link-state.sh | ~/vis-scripts/updateForwardingRate.py H0-R0 utah &
echo Forwarding | ~/vis-scripts/updateState.py R0 utah &

