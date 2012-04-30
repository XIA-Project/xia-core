#!/bin/bash


# for host0
for host in pc328
do
#setup click
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_click_network_evolve_host0.sh"
#setup visualizer
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_visualizer_host0.sh"
# client-side proxy
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_proxy_host0.sh"
done

# for router0
for host in pc267
do
#setup click
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_click_network_evolve_router0.sh"
#setup visualizer
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_visualizer_router0.sh"
done

# for router1
for host in pc323
do
#setup click
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_click_network_evolve_router1.sh"
#setup visualizer
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_visualizer_router1.sh"
done

# for host1
for host in pc333
do
#setup click
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_click_network_evolve_host1.sh"
#setup visualizer
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_visualizer_host1.sh"
# setup service
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_service_host1.sh"
done



echo "Ready!"
