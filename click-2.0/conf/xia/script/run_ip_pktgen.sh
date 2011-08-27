#!/bin/bash
DIR=`dirname \`readlink -m $0\``
sudo click-install -uct24 PAYLOAD_SIZE=$1 -f ${DIR}/../ip_mq_24.click 
