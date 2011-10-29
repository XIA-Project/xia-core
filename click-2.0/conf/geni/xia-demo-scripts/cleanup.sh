#!/bin/bash


# kill click 
for host in pg56 pg55 pg42 pg40
do
ssh -A $host -f "sudo killall -9 click"
done

# kill client-side proxy
for host in pg56
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/kill_proxy_${host}.sh"
done

# kill service
for host in pg40
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/kill_service_${host}.sh"
done

echo "Ready!"