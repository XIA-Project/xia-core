#!/bin/bash

while true;
do
        sleep 1
		
        echo `python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py`
done;