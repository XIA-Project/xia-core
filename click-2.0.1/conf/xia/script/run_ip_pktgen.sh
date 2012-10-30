#!/bin/bash
DIR=`dirname \`readlink -m $0\``
#sudo click-install -uct24 PAYLOAD_SIZE=$1 -f ${DIR}/../ip_mq_12_4_rand.click 
${DIR}/../../../userlevel/click -j 12  PAYLOAD_SIZE=$1  -f ${DIR}/../userlevel/ip_mq_12_4_rand.click 
