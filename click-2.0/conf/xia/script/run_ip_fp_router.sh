#!/bin/bash
DIR=`dirname \`readlink -m $0\``
#sudo ${DIR}/../ip_mq_router4port_t12_noarp_addroute.click
${DIR}/../../../userlevel/click -j 12  ${DIR}/../userlevel/ip_4port_fastpath_addroute.click 
