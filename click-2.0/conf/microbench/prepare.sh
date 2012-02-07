#!/bin/bash

make -C .. clean
make -C ../.. distclean
#pushd ../.. && CPPFLAGS="-g -O2 -fno-omit-frame-pointer" ./conf_user_click_nomt.sh --enable-task-heap && popd
pushd ../.. && CPPFLAGS="-g -O2" ./conf_user_click_nomt.sh --enable-task-heap && popd
make -j4 -C ../.. || exit 1

