#! /bin/bash

cd ./bind-9.8.1
./configure
make
sudo make install

cd ../XIAResolver
make
make python-wrapper

cd ../Register
make test

cd ../bind_demo_conf
sudo cp named.conf /etc/named.conf
sudo mkdir /var/named
sudo cp localhost.zone /var/named/localhost.zone
sudo cp xiaweb.zone /var/named/xiaweb.zone
sudo cp video.zone /var/named/video.zone
sudo cp sidstock.zone /var/named/sidstock.zone
sudo cp xsockconf.ini /var/named/xsockconf.ini

sudo cp hosts_xia /etc/hosts_xia
sudo cp resolv.xiaconf /etc/resolv.xiaconf
