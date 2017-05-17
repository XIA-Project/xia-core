#!/bin/sh

# This script installs MongoDB and its C driver

# Import the MongoDB GPG public key
sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv 7F0CEB10

# Add the MongoDB repository details to APT
echo "deb http://repo.mongodb.org/apt/ubuntu "$(lsb_release -sc)"/mongodb-org/3.0 multiverse" | sudo tee /etc/apt/sources.list.d/mongodb-org-3.0.list

# Make sure os is up to date
sudo apt-get update
sudo apt-get install -y mongodb-org

# Install mongodb c driver
# GENI machines couldn't locate libmongoc, so manually built is used.
# sudo apt-get install -y libmongoc-1.0-0 libbson-1.0
sudo apt-get install -y pkg-config libssl-dev libsasl2-dev
wget https://github.com/mongodb/mongo-c-driver/releases/download/1.4.2/mongo-c-driver-1.4.2.tar.gz
tar xzf mongo-c-driver-1.4.2.tar.gz
cd mongo-c-driver-1.4.2
./configure
make
sudo make install
cd ..
