#!/usr/bin/env python
#
# Copyright 2013 Carnegie Mellon University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
import os
import re
import stat
import hashlib
import xiapyutils
import subprocess

# Directory where all the keys will be dumped
keydir = os.path.join(xiapyutils.xia_srcdir(), 'key')

def generate_rsa_key():
    private = None
    public = None

    DEVNULL = open(os.devnull, 'wb')
    subprocess.check_call("openssl genrsa -out privatekey.txt 1024".split(), stderr=DEVNULL)
    subprocess.check_call("openssl rsa -pubout -in privatekey.txt -out publickey.txt".split(), stderr=DEVNULL)

    with open("privatekey.txt") as privfd:
        private = privfd.read()
    with open("publickey.txt") as pubfd:
        public = pubfd.read()
    os.remove("privatekey.txt")
    os.remove("publickey.txt")
    assert private is not None
    assert public is not None
    return (private, public)

def write_key_files(basename, privkey, pubkey):
	if not os.path.exists(keydir):
		os.mkdir(keydir)
		os.chmod(keydir, 0777)
	privkeyfilename = os.path.join(keydir, basename)
	pubkeyfilename = os.path.join(keydir, basename + '.pub')
	with open(privkeyfilename, 'w') as privkeyfile:
		privkeyfile.write(privkey)
	# Make the private key accessible only to owner
	os.chmod(privkeyfilename, stat.S_IWRITE|stat.S_IREAD)
	with open(pubkeyfilename, 'w') as pubkeyfile:
		pubkeyfile.write(pubkey)

def remove_key_files(key):
	os.remove(os.path.join(keydir, key))
	os.remove(os.path.join(keydir, key+'.pub'))

def create_new_address():
	# Create a new key pair
	(privkey, pubkey) = generate_rsa_key()
	# Create hash of public key to use as device address
	pubkeyhash = pubkey_hash(pubkey)
	# Dump the keys in files based on hash name
	write_key_files(pubkeyhash, privkey, pubkey)
	return pubkeyhash

def create_new_AD():
	return 'AD:' + create_new_address()

def create_new_HID():
	return 'HID:' + create_new_address()

def create_new_SID():
	return 'SID:' + create_new_address()

def create_new_FID():
	return 'FID:' + create_new_address()

def delete_AD(ad):
	adstr, xid = ad.split(':')
	remove_key_files(xid)

def delete_HID(hid):
	hidstr, xid = hid.split(':')
	remove_key_files(xid)

def delete_SID(sid):
	sidstr, xid = sid.split(':')
	remove_key_files(xid)

def pubkey_hash(pubkey):
	keylist = pubkey.split('\n')
	# Strip the first and last line because they just contain text markers
	key = ''.join(keylist[1:-2])
	return hashlib.sha1(key).hexdigest()

if __name__ == "__main__":
	print create_new_address()
