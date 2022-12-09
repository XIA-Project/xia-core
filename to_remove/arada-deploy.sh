#!/bin/bash
#

BUILDROOT=/opt/buildroot-2013.11/output/host/usr
SANDBOX=arada_deploy_sandbox
XIADIR=xia-core

BUILDROOTBIN=$BUILDROOT/bin
BUILDROOTLIB=$BUILDROOT/lib
BUILDROOTXIA=$BUILDROOT/$XIADIR
SANDBOXBIN=$SANDBOX/bin
SANDBOXLIB=$SANDBOX/lib
SANDBOXXIA=$SANDBOX/$XIADIR
ORIGDIR=`pwd`

# Make sure we are running from xia-core
CURRENTDIR=`pwd`
CURRENTDIRNAME=`basename $CURRENTDIR`
if [ "$CURRENTDIRNAME" != "$XIADIR" ]; then
	echo "Run this script from xia-core directory"
	exit -1
fi

# The user is required to provide IP address of the target
if [ "$1" == "" ]; then
	echo "Usage: $0 <Arada_Target_IP_Addr>"
	exit -2
fi

if [ ! -f click/userlevel/click ]; then
	echo "Click executable not found. Please build XIA. Aborting"
	exit -3
fi

# prepare a directory tree to send over to target
echo "Preparing files to be sent to target"
rm -fr $SANDBOX
mkdir $SANDBOX
if [ $? -ne 0 ]; then
	echo "Failed creating $SANDBOX"
	exit -1
fi
mkdir -p $SANDBOXBIN
if [ $? -ne 0 ]; then
	echo "Failed creating$SANDBOXBIN"
	exit -1
fi
mkdir -p $SANDBOXLIB
if [ $? -ne 0 ]; then
	echo "Failed creating $SANDBOXLIB"
	exit -1
fi
mkdir -p $SANDBOXXIA
if [ $? -ne 0 ]; then
	echo "Failed creating $SANDBOXXIA"
	exit -1
fi

pushd $BUILDROOTBIN > /dev/null
if [ $? -ne 0 ]; then
	echo "Failed to switch to $BUILDROOTBIN"
	exit -1
fi
cp python* openssl tr bash $ORIGDIR/$SANDBOXBIN
if [ $? -ne 0 ]; then
	echo "Failed copying binaries from $BUILDROOTBBIN"
	exit -5
fi
popd # $BUILDROOTBIN

cp -R $BUILDROOTLIB/* $SANDBOXLIB
if [ $? -ne 0 ]; then
	echo "Failed copying libraries from $BUILDROOTLIB"
	exit -6
fi

cp -R $BUILDROOTXIA/* $SANDBOXXIA
if [ $? -ne 0 ]; then
	echo "Failed copying XIA to $SANDBOXXIA"
	exit -7
fi

cd $SANDBOX
echo "Deploying files on target"
scp -r * root@$1:/tmp/usb/
if [ $? -ne 0 ]; then
	echo "Failed copying everything to target"
	exit -11
fi
cd ..

echo ""
echo "All files successfully deployed on target."
echo ""
echo "Run these commands on target to setup environment to run XIA"
echo "export LD_LIBRARY_PATH=/tmp/usb/lib/:/tmp/usb/xia-core/api/lib/"
echo "export PATH=\$PATH:/tmp/usb/bin"
echo "echo 127.0.1.1 \`hostname\` >> /etc/hosts"
echo "mkdir -p /tmp/usb/xia-core/key"
