#!/bin/bash
#

ORIGPATH=$PATH

echo "***"
echo "*** NOTE: Do not store arada stuff on a public location"
echo "***"
sleep 5
if [ ! -f arada.tar.gz ]; then
	echo "ERROR: arada.tar.gz not in current directory. Aborting"
	exit -1
else
	echo "Found arada tarball"
fi

# Unpack the arada tarball
if [ -d arada ]; then
	echo "arada directory exists. Skipped unpacking arada.tar.gz"
else
	tar xzf arada.tar.gz
	if [ $? -ne 0 ]; then
		echo "ERROR: Unable to unpack arada.tar.gz. Aborting"
		exit -2
	else
		echo "Unpacked arada tarball"
	fi
fi

# Verify we have the toolchain tarball
pushd arada/toolchain

if [ ! -f LocoMate_tlcn_64bit.tar.bz2 ]; then
	echo "ERROR: toolchain tarball missing: LocoMate_tlcn_64bit.tar.bz2"
	exit -3
else
	echo "LocoMate 64-bit toolchain tarball found"
fi

# Verify that the toolchain is not already installed
if [ -d "/opt/buildroot-2013.11" ]; then 
	echo "Toolchain already in /opt/buildroot-2013.11. Skipping..."
else
	echo "Installing toolchain in /opt/buildroot-2013.11"

	# Install the toolchain
	echo "Installing toolchain using sudo. Need your password:"
	sudo tar -Pjxvf LocoMate_tlcn_64bit.tar.bz2 &> locomate_unpack.log
	if [ $? -ne 0 ]; then
		echo "ERROR: Failed to unpack locomate toolchain"
		exit -5
	else
		echo "Toolchain installation complete"
	fi
fi

# Return to the directory we started from
popd #arada/toolchain

# Now check if protobuf is installed on the host system
if [ ! -f "/usr/bin/protoc" ]; then
	echo "Please install protubuf-compiler, libprotobuf-dev and python-protobuf"
	echo "ERROR: /usr/bin/protoc not found. Aborting"
	exit -6
fi

# Build protobuf for mips in a sandbox
if [ -d arada/sandbox-protobuf ]; then
	echo "Protobuf: sandbox exists. Cross-compile skipped..."
else
	echo "Protobuf: creating sandbox"
	mkdir -p arada/sandbox-protobuf
	if [ ! -d arada/sandbox-protobuf ]; then
		echo "ERROR: Unable to create arada/sandbox-protobuf. Aborting"
		exit -7
	fi
	pushd arada/sandbox-protobuf

	echo "Protobuf: retrieving source from distro"
	apt-get source protobuf &> protobuf_source.log
	if [ $? -ne 0 ]; then
		echo "ERROR: retrieving source package for protobuf. Aborting"
		exit -8
	fi

	echo "Protobuf: preparing to build"
	pushd protobuf-*
	if [ $? -ne 0 ]; then
		echo "ERROR: No/multiple protobuf-* directory. Aborting"
		exit -9
	fi
	echo "Protobuf: configuring the build"
	PROTOCPATH=`which protoc`
	export PATH=/opt/buildroot-2013.11/output/host/usr/bin/:$PATH
	./configure --host=mips-linux --prefix=/opt/buildroot-2013.11/output/host/usr/ --with-protoc=$PROTOCPATH &> protobuf_configure.log
	echo "Protobuf: building"
	make &> protobuf_cross_build.log
	if [ $? -ne 0 ]; then
		echo "Protobuf cross-compile failed"
		exit -10
	fi

	# This can fail, ignore failures
	make check  &> protobuf_make_check.log

	echo "Protobuf: installing"
	sudo PATH=$PATH make install &> protobuf_cross_install.log
	if [ $? -ne 0 ]; then
		echo "Installation of protobuf failed"
		exit -11
	fi
	popd # protobuf*
	popd #arada/sandbox-protobuf
	export PATH=$ORIGPATH
fi

# Create a sandbox for openssl source code
if [ -d arada/sandbox-openssl ]; then
	echo "OpenSSL: sandbox exists. Cross-compile skipped..."
else
	echo "OpenSSL: creating sandbox"
	mkdir -p arada/sandbox-openssl
	if [ $? -ne 0 ]; then
		echo "Failed creating arada/sandbox-openssl"
		exit -12
	fi
	pushd arada/sandbox-openssl

	echo "OpenSSL: retrieving source from distro"
	apt-get source openssl &> openssl_source.log
	if [ $? -ne 0 ]; then
		echo "Failed getting openssl source package for your distro"
		exit -13
	fi

	echo "OpenSSL: preparing to build"
	pushd openssl-*
	echo "OpenSSL: configuring the build"
	export CC=/opt/buildroot-2013.11/output/host/usr/bin/mips-linux-gcc
	./Configure shared no-ssl2 --prefix=/opt/buildroot-2013.11/output/host/usr/ linux-mips32 &> openssl_configure.log
	echo "OpenSSL: building"
	make &> openssl_cross_build.log
	if [ $? -ne 0 ]; then
		echo "ERROR: Failed building openssl"
		exit -14
	fi
	echo "OpenSSL: installing"
	sudo PATH=$PATH make install &> openssl_cross_install.log
	if [ $? -ne 0 ]; then
		echo "ERROR: Failed installing openssl"
		exit -15
	fi
	popd #openssl-*
	popd #arada/sandbox-openssl
	unset CC
fi

# Now build python
if [ -d arada/sandbox-python ]; then
	echo "Python: sandbox exists. Crons-compile skipped..."
else
	echo "Python: creating sandbox"
	mkdir -p arada/sandbox-python
	if [ $? -ne 0 ]; then
		echo "Failed creating arada/sandbox-python"
		exit -16
	fi
	pushd arada/sandbox-python

	echo "Python: retrieving source from distro"
	apt-get source python2.7 &> python_source.log
	if [ $? -ne 0 ]; then
		echo "Failed getting python2.7 source package for your distro"
		exit -17
	fi

	echo "Python: preparing to build"
	pushd python2.7-*
	echo "Python: configuring the host build"
	# TODO: Reset to original path without /opt/buildroot-
	./configure &> python_host_configure.log
	echo "Python: building for host"
	make python Parser/pgen &> python_host_build.log
	echo "Python: saving off host binaries. Will not use"
	mv python python_for_build
	mv Parser/pgen{,_for_build}
	echo "Python: enabling additional modules"
	sed "s/if ext.name in sys.builtin_module_names:/if ext.name in ('__builtin__', '__main__', '_ast', '_codecs', '_sre', '_symtable', '_warnings', '_weakref', 'errno', 'exceptions', 'gc', 'imp', 'marshal', 'posix', 'pwd', 'signal', 'sys', 'thread', 'xxsubtype', 'zipimport'):/" setup.py > setup.py.new
	mv setup.py setup.py.orig
	mv setup.py.new setup.py
	echo "Python: configuring for cross-compile"
	export PATH=$PATH:/opt/buildroot-2013.11/output/host/usr/bin/
	./configure --host=mips-linux --build=x86_64-linux-gnu --prefix=/opt/buildroot-2013.11/output/host/usr/ --disable-ipv6 ac_cv_file__dev_ptmx=no ac_cv_file__dev_ptc=no ac_cv_have_long_long_format=yes &> python_configure.log
	echo "Python: building"
	make CFLAGS="-g0 -s -O2 -march=24kc -fomit-frame-pointer -fPIC -fdata-sections -ffunction-sections -pipe -L/opt/buildroot-2013.11/output/host/usr/lib/" &> python_cross_build.log
	echo "Python: installing"
	sudo PATH=$PATH make install &> python_cross_install.log

	popd #python-*
	popd #arada/sandbox-python
	export PATH=$ORIGPATH
fi

echo "Copying Arada wave headers to /opt/buildroot"
sudo cp -ax arada/code/include /opt/buildroot-2013.11/output/host/usr/include/arada
if [ $? -ne 0 ]; then
	echo "ERROR: copying Arada headers to /opt/buildroot.../usr/include/arada"
	exit -20
fi

echo "Copying Arada wave libraries to /opt/buildroot"
sudo cp -ax arada/code/mips/lib/* /opt/buildroot-2013.11/output/host/usr/lib/
if [ $? -ne 0 ]; then
	echo "ERROR: copying Arada libs to /opt/buildroot.../usr/lib/"
	exit -21
fi

echo "Cross-compile toolchain setup complete."
echo ""
echo ""
echo "XIA can now be built with the following commands in xia-core:"
echo "export PATH=\$PATH:/opt/buildroot-2013.11/output/host/usr/bin/"
echo "tarch=mips ./configure; make"
