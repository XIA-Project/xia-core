#!/bin/bash

if [ ! -d "android-deps/protobuf" ]; then
  svn co -r378 http://protobuf.googlecode.com/svn/trunk/ android-deps/protobuf
fi

already_patched=$(grep CLICK_ANDROID click/include/click/config.h | wc -l)
if [ $already_patched -eq 0 ]; then 
  patch -p0 < android-deps/config_patch.diff
fi
