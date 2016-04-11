#!/bin/bash

# update XIA code (from git repo)
for host in pc201 pc222 pc211 pc204
do
ssh -A $host "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/code_update.sh"
done


#setup BBN visualizer DB
for host in pc201
do
ssh -A $host -f "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/setup_visualizer_DB_host0.sh"
done

echo "Ready!"
