#!/bin/bash

cd ../../../..
git pull
cd click-2.0
make elemlist
make
cd ../XIASocket/API
make
cd ../sample
make

