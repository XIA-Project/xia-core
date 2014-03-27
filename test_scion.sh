#!/usr/bin/env bash
./bin/xianet kill
./configure
make -j3
#./bin/xianet -s xia_scion_topology.click -V -l 7 start
./bin/xianet -s xia_scion_topology.click -V start
