#!/usr/bin/env bash
./bin/xianet kill
./configure
make
./bin/xianet -V -l 7 start
