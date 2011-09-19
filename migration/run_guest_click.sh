#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

rsync -av -e "ssh -i $PREFIX/id_rsa_vm -p 5555" $CLICK_PATH/userlevel/click $CLICK_PATH/conf/xia/{xia_router_template.click,xia_address.click,xia_vm_common.click,xia_vm_ping_server.click} root@127.0.0.1:~/

ssh -i $PREFIX/id_rsa_vm -p 5555 root@127.0.0.1 "bash -c \"killall click; ./click xia_vm_ping_server.click > stdout; exit\"" &

