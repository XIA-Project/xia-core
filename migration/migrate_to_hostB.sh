#!/bin/bash

PREFIX="`dirname $0`"
PREFIX="`readlink -f $PREFIX`"
source $PREFIX/common.sh

echo "migrate -d tcp:$HOST_B_IP:6666" | netcat -q 1 127.0.0.1 4444

