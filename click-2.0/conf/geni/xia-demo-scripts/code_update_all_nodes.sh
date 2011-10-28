#!/bin/bash


for host in pg40 pg56 pg42 pg55
do
ssh -A $host "~/xia-core/click-2.0/conf/geni/xia-demo-scripts/sub_scripts/code_update.sh"
done

