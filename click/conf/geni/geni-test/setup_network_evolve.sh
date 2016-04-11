#!/bin/bash


#setup click
for host in pg56 pg55 pg42 pg40
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_click_network_evolve_${host}.sh"
done

sleep 1

#setup visualizer
for host in pg56 pg55 pg42 pg40
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_visualizer_${host}.sh"
done

# client-side proxy
for host in pg56
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_proxy_${host}.sh"
done

# service
for host in pg40
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_service_${host}.sh"
done

echo "Ready!"