#!/bin/bash

set -e
sudo cp ./xia-core/experiments/planetlab/libprotobuf.so.8 /usr/local/lib/
sudo cp ./xia-core/experiments/planetlab/libc.conf /etc/ld.so.conf.d/
sudo /sbin/ldconfig