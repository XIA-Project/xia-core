#!/bin/bash


prev_total=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py | awk -F " " '{print $1}'`
prev_cid=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py | awk -F " " '{print $3}'`
prev_sid=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py | awk -F " " '{print $2}'`
prev_hid=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py | awk -F " " '{print $4}'`

while true;
do
        sleep 1

		now_total=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py | awk -F " " '{print $1}'`
		now_cid=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py | awk -F " " '{print $3}'`
		now_sid=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py | awk -F " " '{print $2}'`
		now_hid=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py | awk -F " " '{print $4}'`
		
		
		total=`expr $now_total - $prev_total`
		cid=`expr $now_cid - $prev_cid`
		sid=`expr $now_sid - $prev_sid`
		hid=`expr $now_hid - $prev_hid`
		
        prev_total=$now_total
		prev_sid=$now_sid
		prev_cid=$now_cid
		prev_hid=$now_hid

        echo $total $cid $sid $hid 
done;