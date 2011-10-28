#!/bin/bash

cd ~/xia-core
git pull
cd click-2.0
sudo make
cd ../XIASocket/API
make
cd ../sample
make

