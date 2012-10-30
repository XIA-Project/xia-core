#!/bin/bash

prev_list=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py`
arr=()
for x in $prev_list
do
  arr=("${arr[@]}" $x)
done
 
prev_total=${arr[0]}
prev_sid=${arr[1]}
prev_cid=${arr[2]}
prev_hid=${arr[3]}

while true;
do
        sleep 1

		cur_list=`python ~/xia-core/click-2.0/conf/geni/stats/read_stats.py`
		arr=()
		for x in $cur_list
		do
			arr=("${arr[@]}" $x)
		done
 
		cur_total=${arr[0]}
		cur_sid=${arr[1]}
		cur_cid=${arr[2]}
		cur_hid=${arr[3]}
		
		
		total=`expr $cur_total - $prev_total`
		cid=`expr $cur_cid - $prev_cid`
		sid=`expr $cur_sid - $prev_sid`
		hid=`expr $cur_hid - $prev_hid`
		
		if [  "$total" -lt 0 ]; then
				total=0
				prev_total=0
		else 
			prev_total=$cur_total
		fi

		if [  "$cid" -lt 0 ]; then
				cid=0
				prev_cid=0
		else 
			prev_cid=$cur_cid
		fi
		
		if [  "$sid" -lt 0 ]; then
				sid=0
				prev_sid=0
		else 
			prev_sid=$cur_sid
		fi

		if [  "$hid" -lt 0 ]; then
				hid=0
				prev_hid=0
		else 
			prev_hid=$cur_hid
		fi												
		
		total=`expr $cid + $sid`

        echo $total $cid $sid 0 
done;
