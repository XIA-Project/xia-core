#!/bin/bash -x
for iter in {1..10}
do
  for f in  "intra" "serial" 
  do
    for i in {0..3}
    do
      bash -c "time ../../userlevel/click -j10 xia_packetforward_fallback${i}_${f}.click" &>> ${f}${i}
      echo "" >>  ${f}${i}
      echo "" >>  ${f}${i}
    done
  done
done

