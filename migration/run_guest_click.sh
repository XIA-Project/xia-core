#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

scp -i id_rsa_vm $CLICK_PATH/userlevel/click -P 5555 $CLICK_PATH/conf/xia/xia_vm_guest.click root@127.0.0.1:~/

ssh -i $PREFIX/id_rsa_vm -p 5555 root@127.0.0.1 bash -c "sudo killall click; ./click xia_ping_server.click"

