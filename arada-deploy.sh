#!/bin/bash
#

BUILDROOT=/opt/buildroot-2013.11/output/host
SANDBOX=arada_deploy_sandbox
XIADIR=xia-core

BUILDROOTBIN=$BUILDROOT/usr/bin
BUILDROOTLIB=$BUILDROOT/usr/lib
SANDBOXBIN=$SANDBOX/bin
SANDBOXLIB=$SANDBOX/lib
SANDBOXXIAAPILIB=$SANDBOX/$XIADIR/api/lib
SANDBOXXIABIN=$SANDBOX/$XIADIR/bin

TARBALL=$SANDBOX.tar.gz

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
mkdir -p $SANDBOXBIN
mkdir -p $SANDBOXLIB
mkdir -p $SANDBOXXIAAPILIB
mkdir -p $SANDBOXXIABIN
cp $BUILDROOTBIN/python* $SANDBOXBIN
if [ $? -ne 0 ]; then
	echo "Failed copying binaries from $BUILDROOTBBIN"
	exit -5
fi

cp -R $BUILDROOTLIB/* $SANDBOXLIB
if [ $? -ne 0 ]; then
	echo "Failed copying libraries from $BUILDROOTLIB"
	exit -6
fi

cp -R api/lib/* $SANDBOXXIAAPILIB
if [ $? -ne 0 ]; then
	echo "Failed copying XIA libraries to $SANDBOXXIAAPILIB"
	exit -7
fi

cp -R bin/* $SANDBOXXIABIN
if [ $? -ne 0 ]; then
	echo "Failed copying XIA executables to $SANDBOXXIABIN"
	exit -8
fi


cp click/userlevel/click $SANDBOXBIN
if [ $? -ne 0 ]; then
	echo "Failed copying click executable"
	exit -9
fi

cd $SANDBOX
tar -czf ../$TARBALL *
if [ $? -ne 0 ]; then
	echo "Failed creating tarball for target"
	exit -10
fi
cd .. #sandbox

echo "Copying files to target."
echo "Provide root@$1 password when prompted."
sleep 1

scp $TARBALL root@$1:/tmp/usb/
if [ $? -ne 0 ]; then
	echo "Failed copying tarball to target"
	exit -11
fi

echo "Deploying files on target"
ssh root@$1 "cd /tmp/usb; tar -xzf $TARBALL"
if [ $? -ne 0 ]; then
	echo "Failed unpacking tarball on target"
	exit -12
fi

# Clean up the sandbox
rm -fr $SANDBOX
rm $TARBALL

echo "All files successfully deployed on target."
