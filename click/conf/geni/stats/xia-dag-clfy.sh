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

clfy=1
echo $clfy

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
		
		
		if [ "$sid" -gt "$cid" ]; then
			if [  "$clfy" == 1 ]; then
				clfy=1
			else
				clfy=1
				echo $clfy
			fi
		elif [ "$cid" -gt "$sid" ]; then
			if [  "$clfy" == 2 ]; then
				clfy=2
			else
				clfy=2
				echo $clfy
			fi  
		else 
			clfy=$clfy
		fi
		
done;