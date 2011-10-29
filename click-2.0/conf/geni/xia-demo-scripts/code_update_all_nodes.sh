#!/bin/bash

# update XIA code (from git repo)
for host in pg40 pg56 pg42 pg55
do
ssh -A $host "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/code_update.sh"
done


#setup BBN visualizer DB
for host in pg56 
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_visualizer_DB_${host}.sh"
done

echo "Ready!"