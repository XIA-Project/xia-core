#!/usr/bin/env bash
./bin/xianet kill
./configure
make
./bin/xianet -s xia_intradomain_routing_topology.click -V -l 5 start
