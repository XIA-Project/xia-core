#!/bin/bash
EXPECTED_ARGS=2
E_BADARGS=65

if [ $# -ne $EXPECTED_ARGS ]
then
  echo "Usage: `basename $0` payloadsize num_cpus"
  exit $E_BADARGS
fi
DIR=`dirname \`readlink -m $0\``
${DIR}/../../../userlevel/click PAYLOAD_SIZE=$1 -j 12  ${DIR}/../userlevel/xia_mq_isolate$2.click
