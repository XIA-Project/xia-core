#!/usr/bin/env bash
./bin/xianet kill
./configure
make
./bin/xianet -s xia_interdomain_routing_topology.click -V -l 6 start
