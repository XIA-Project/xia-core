#!/bin/bash

export LD_LIBRARY_PATH=:/usr/local/lib:.

sudo killall -9 stock_service.py
sudo killall -9 hello_service.py
sudo killall -9 vs
sudo killall -9 webserver.py

sleep 1
cd ~/xia-core/web_demo
rm xsockconf_python.ini
./stock_service.py >& /dev/null &
./hello_service.py >& /dev/null &

sleep 1
cd ~/xia-core/XIASocket/sample
rm xsockconf.ini
./vs video.ogv >& /dev/null &

sleep 1

cd ~/xia-core/web
rm xsockconf_python.ini
./webserver.py >& /dev/null &

