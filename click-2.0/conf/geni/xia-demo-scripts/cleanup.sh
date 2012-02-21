#!/bin/bash


# kill click 
for host in pc201 pc222 pc211 pc204
do
ssh -A $host -f "sudo killall -9 click"
done

# kill client-side proxy
for host in pc201
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/kill_proxy_host0.sh"
done

# kill service
for host in pc204
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/kill_service_host1.sh"
done

echo "Ready!"
