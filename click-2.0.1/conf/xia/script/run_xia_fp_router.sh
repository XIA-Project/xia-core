#!/bin/bash
DIR=`dirname \`readlink -m $0\``
#sudo ${DIR}/../xia_ipencap_four_port_mq_router.click
${DIR}/../../../userlevel/click -j 12  ${DIR}/../userlevel/xia_ipencap_four_port_fastpath_mq_router.click
