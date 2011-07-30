#!/bin/sh

cd `dirname $0`/
./configure --enable-linuxmodule --enable-warp9 --enable-multithread=4 --disable-userlevel

