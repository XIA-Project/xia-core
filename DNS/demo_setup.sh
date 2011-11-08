#! /bin/bash

cd ./bind-9.8.1
./configure
make
sudo make install

cd ../XIAResolver
make
make python-wrapper

cd ../bind_demo_conf
sudo cp named.conf /etc/named.conf
sudo cp localhost.zone /var/named/localhost.zone
sudo cp xiaweb.zone /var/named/xiaweb.zone
sudo cp video.zone /var/named/video.zone
sudo cp sidstock.zone /var/named/sidstock.zone
