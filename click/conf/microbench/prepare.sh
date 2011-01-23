#!/bin/bash

make -C .. clean
make -C ../.. distclean
pushd ../.. && CXXFLAGS="-g -O2 -fno-omit-frame-pointer" ./configure --enable-task-heap && popd
make -j4 -C ../.. || exit 1

