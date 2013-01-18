#!/bin/bash

PREFIX=`dirname $0`
PREFIX=`readlink -f $PREFIX`
source $PREFIX/common.sh

ssh -i $PREFIX/id_rsa_vm -p 5555 root@127.0.0.1 "$@"

