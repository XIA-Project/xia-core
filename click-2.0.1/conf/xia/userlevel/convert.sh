#!/bin/bash
cat $1  | sed -e '1d'  | sed 's/eth2/xge0/g' | sed 's/eth3/xge1/g' | sed 's/eth4/xge2/g' | sed 's/eth5/xge3/g'
