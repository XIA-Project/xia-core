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
mkdir ${OUTDIR} &> /dev/null
OUTDIR=${OUTDIR}/$1
mkdir ${OUTDIR} &> /dev/null
POSTFIX=`date +%b%d-%H:%M:%S`
interval=$2

function ethtool {
  dev=$1
  outfile=$2
  date +%b%d-%H:%M:%S > $outfile
  sudo ethtool -S $dev &>> $outfile
}

for i in $(seq 0 3)
do
  dev="xge$i"
  ethtool $dev "${OUTDIR}/ethtool-begin-${dev}-${POSTFIX}"
done

#${DIR}/../../click_mq_stats.rb 120  > ${OUTDIR}/stat-${POSTFIX} 
${DIR}/../../interface_stat.rb 120  > ${OUTDIR}/stat-${POSTFIX} 

for i in $(seq 0 3)
do
  dev="xge$i"
  ethtool $dev "${OUTDIR}/ethtool-end-${dev}-${POSTFIX}"
done
