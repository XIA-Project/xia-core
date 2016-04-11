#!/bin/bash

#bash

# Install protobuf
cd ~/
sudo wget http://protobuf.googlecode.com/files/protobuf-2.3.0.tar.gz
tar zxf protobuf-2.3.0.tar.gz
cd protobuf-2.3.0/
sudo ./configure --prefix=/usr
sudo make
sudo make install

sudo apt-get -y install protobuf-compiler

# Install python-dev
sudo apt-get -y install python-dev

# Install swig
sudo apt-get -y install swig

# Install python-argparse
sudo apt-get -y install python-argparse

# Add "." into LD_LIBRARY_PATH
cd ~/
echo export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:. >> .bashrc
source .bashrc


# Install mysql-python for GENI Visualizer
sudo aptitude -y install python-setuptools
sudo apt-get -y install libmysql++-dev
wget http://downloads.sourceforge.net/project/mysql-python/mysql-python/1.2.3/MySQL-python-1.2.3.tar.gz
tar xfz MySQL-python-1.2.3.tar.gz
cd MySQL-python-1.2.3
python setup.py build
sudo python setup.py install


# Download XIA-prototype
cd ~/
sudo wget http://www.xia.cs.cmu.edu/wiki/xia-proto/xia-v0.91.tar.gz 
tar zxf xia-v0.91.tar.gz

# Build XIA-prototype
cd xia-core/tools/
./buildxia

# Delete xsockconf.ini files that are not for GENI XIA-prototype
cd ~/
cd xia-core/XIASocket/sample
rm xsockconf.ini

cd ~/
cd xia-core/web
rm xsockconf_python.ini

cd ~/
cd xia-core/web_demo
rm xsockconf_python.ini

# Generate host/router click script
cd ~/
cd xia-core/click-2.0/conf/geni/init-scripts/template
python xconfig.py
python xconfig.py -r






