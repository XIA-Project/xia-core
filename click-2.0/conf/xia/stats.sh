cd /click

for d in `seq 2 5`;
do
  for f in `seq 0 11`;
  do
    echo -n "eth${d} "
    echo -n $f
    echo -n " "
    echo `cat pd_eth${d}_$f/count` `cat tod_eth${d}_$f/count`
  done
  echo ""
done
