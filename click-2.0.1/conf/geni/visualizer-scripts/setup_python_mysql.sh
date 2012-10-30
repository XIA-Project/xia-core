#!/bin/bash


sudo aptitude -y install python-setuptools

sudo apt-get -y install libmysql++-dev

wget http://downloads.sourceforge.net/project/mysql-python/mysql-python/1.2.3/MySQL-python-1.2.3.tar.gz

tar xfz MySQL-python-1.2.3.tar.gz

cd MySQL-python-1.2.3

python setup.py build

sudo python setup.py install
