#!/bin/bash

PREFIX="`dirname $0`"
PREFIX="`readlink -f $PREFIX`"
source $PREFIX/common.sh

echo "info migrate" | netcat -q 1 127.0.0.1 4444

