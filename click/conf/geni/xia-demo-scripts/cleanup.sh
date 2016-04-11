#!/bin/bash


# kill click 
for host in pc328 pc267 pc323 pc333
do
ssh -A $host -f "sudo killall -9 click"
done

# kill client-side proxy
for host in pc328
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/kill_proxy_host0.sh"
done

# kill service
for host in pc333
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/kill_service_host1.sh"
done

echo "Ready!"
