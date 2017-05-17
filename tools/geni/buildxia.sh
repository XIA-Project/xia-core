#!/bin/sh
# Script to deploy XIA on Ubuntu 11 and above
# 4/4/13 updated for XIA 1.0
# 10/4/13 updated for revised XIA build commands

# do this or flack will install into /
cd ~

# Import the MongoDB GPG public key
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 7F0CEB10

# add the MongoDB repository details to APT
echo "deb http://repo.mongodb.org/apt/ubuntu "$(lsb_release -sc)"/mongodb-org/3.0 multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-3.0.list

# make sure os is up to date
sudo apt-get update
sudo apt-get -y upgrade

# Install required packages
sudo apt-get install -y git g++ make openssl
sudo apt-get install -y libprotobuf-dev protobuf-compiler
sudo apt-get install -y python-tk python-dev python-setuptools
sudo apt-get install -y swig
sudo apt-get install -y python-argparse
sudo apt-get install -y python-pygame
sudo apt-get install -y mongodb-org

# for some reason this won't install via apt-get on the GENI ubuntu 11 nodes
sudo easy_install requests

# get XIA source & build
git clone https://github.com/XIA-Project/xia-core.git
cd xia-core
git checkout develop

./configure
make
cd ..

# Install mysql-python for GENI Visualizer
#sudo apt-get -y install libmysql++-dev
#if [ ! -f MySQL-python-1.2.3.tar.gz ]; then 
#	wget http://downloads.sourceforge.net/project/mysql-python/mysql-python/1.2.3/MySQL-python-1.2.3.tar.gz
#fi
#tar xfz MySQL-python-1.2.3.tar.gz
#cd MySQL-python-1.2.3
#python setup.py build
#sudo python setup.py install
