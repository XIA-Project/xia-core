#!/bin/bash
EXPECTED_ARGS=1
E_BADARGS=65

if [ $# -ne $EXPECTED_ARGS ]
then
  echo "Usage: `basename $0` OUTPUT-DIRNAME "
  exit $E_BADARGS
fi

DIR=`dirname \`readlink -m $0\``
OUTDIR=${DIR}/output
mkdir ${OUTDIR} -f 2&>1 > /dev/null
OUTDIR=${OUTDIR}/$1
mkdir ${OUTDIR} -f 2&>1 > /dev/null
OUTFILE=${OUTDIR}/`date +%b%d-%H:%M:%S`
interval=$2

function ethtool {
  dev= $1
  outfile=$2
  sudo ethtool -S $dev 2&>1 > $outfile
}

for i in $(seq 2 5)
do
  ethtool($dev, ethtool-start-$dev-$OUTFILE)
done

${DIR}/../../click_mq_stats.rb 60 2&>1  > stat-${OUTFILE} 

for i in $(seq 2 5)
do
  dev=eth$i
  ethtool($dev, ethtool-end-$dev-$OUTFILE)
done
