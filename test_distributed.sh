#!/usr/bin/env bash
sudo ./bin/xianet kill
./configure
make
etc/click/read_distributed_topology.py
sudo ./bin/xianet -s xia_topology_local.click -V -l 7 start
