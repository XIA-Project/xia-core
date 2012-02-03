#!/bin/bash

killall -9 stock_service.py
killall -9 vs
killall -9 proxy.py 
killall -9 webserver.py
killall -9 hello_service.py

killall -9 stock_service_replicate.py
killall -9 webserver_replicate.py

killall -9 ps_beacon
killall -9 pc_beacon

killall -9 check_beacon.py
