#!/bin/bash


sudo killall -9 click

killall -9 webserver_replicate_cmu.py
killall -9 stock_service_replicate_cmu.py

killall -9 pc_beacon

sudo ../../../userlevel/click ../cmu-server.click &

sleep 1
cd ../../../../web_demo/

./stock_service_replicate_cmu.py &

cd ../proxies
./webserver_replicate_cmu.py $@  &



#run ps_beacon at pg56
#ssh -A mberman@pg56.emulab.net -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_beacon_pg56.sh"


sleep 2

cd ../XIASocket/sample/
./pc_beacon &


#run check_beacon at router1
ssh -A sblee@pc211.emulab.net -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/check_beacon_router1.sh"



#sudo killall -9 updateForwardingRate.py

#sleep 1
#sh ~/xia-core/click-2.0/conf/geni/stats/xia-link-state-cmu.sh | ~/xia-core/click-2.0/conf/geni/visualizer-scripts/updateForwardingRate.py R1-CMU ganel.gpolab.bbn.com &


echo "Ready!"


