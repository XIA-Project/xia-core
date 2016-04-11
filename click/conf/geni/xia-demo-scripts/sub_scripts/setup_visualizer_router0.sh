#!/bin/bash

sudo killall -9 updateForwardingRate.py
sudo killall -9 updateState.py
sleep 1
python ~/xia-core/click-2.0/conf/geni/visualizer-scripts/prep1.py
bash ~/xia-core/click-2.0/conf/geni/stats/xia-link-state.sh |  ~/xia-core/click-2.0/conf/geni/visualizer-scripts/updateForwardingRate.py H0-R0 ganel.gpolab.bbn.com &
echo Forwarding |  ~/xia-core/click-2.0/conf/geni/visualizer-scripts/updateState.py R0 ganel.gpolab.bbn.com &

