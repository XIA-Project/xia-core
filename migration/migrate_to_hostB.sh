#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

#echo preparing the target host
ssh $HOST_B_IP $COMMON_PREFIX/listen_for_incoming_guest.sh

#echo requesting migration
$PREFIX/qemu_monitor_cmd.py $HOST_A_IP 4444 "migrate tcp:$HOST_B_IP:6666"
#$PREFIX/qemu_monitor_cmd.py $HOST_A_IP 4444 "migrate -d tcp:$HOST_B_IP:6666"

#echo migration result
#while true; do
#	RESULT=`$PREFIX/qemu_monitor_cmd.py $HOST_A_IP 4444 "info migrate"`
#	echo "$RESULT"
#	echo
#	if [ `echo "$RESULT" | grep active | wc -l` == "0" ]; then break; fi
#	sleep 0.1
#done

#echo send an update to guest
sudo $CLICK_PATH/userlevel/click $CLICK_PATH/conf/xia/xia_vm_ping_update_hostB.click

#echo done

