#!/bin/bash


sudo killall -9 click

killall -9 webserver_replicate_cmu.py
killall -9 stock_service_replicate_cmu.py

killall -9 pc_beacon

sudo killall -9 updateForwardingRate.py

echo "Ready!"


