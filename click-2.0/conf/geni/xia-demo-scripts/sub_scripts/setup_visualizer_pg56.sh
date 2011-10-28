#!/bin/bash

sudo killall -9 updateGraphic.py
sudo killall -9 prep1.py
sleep 3
cd ~/vis-scripts
./prep1.py
echo 3 | ~/vis-scripts/updateGraphic.py utah &
