#!/bin/bash
DIR=`dirname \`readlink -m $0\``
sudo click-install -uct24 $PAYLOAD_SIZE=$1 ${DIR}/../xia_mq_24.click 
