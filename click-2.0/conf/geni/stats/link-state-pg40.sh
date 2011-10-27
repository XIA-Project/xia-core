#!/bin/bash

prev=`netstat -i | grep eth3 | awk -F " " '{print $8}'`

while true;
do
        sleep 1
        now=`netstat -i | grep eth3 | awk -F " " '{print $8}'`
        tx=`expr $now - $prev`
        prev=$now

        echo $tx
done;