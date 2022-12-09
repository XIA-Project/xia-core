#!/bin/bash
cd /home/cmu_xia/fedora-bin/xia-core/applications/web
# checks that web is not running before trying to run
sudo killall webserver.py
./webserver.py $1 $2 $3 $4