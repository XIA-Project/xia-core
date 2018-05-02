#!/usr/bin/env python2.7
#

# Minimal Publisher

import os
import sys
import argparse

srcdir = os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]
sys.path.append(os.path.join(srcdir, "daemons/xcache/publisher"))
sys.path.append(os.path.join(srcdir, "bin"))

import xiapyutils
from subprocess import check_call

REQ_TEMPLATE="""
[ req ]
default_bits		= 1024
default_keyfile 	= keyfile.pem
distinguished_name	= req_distinguished_name
prompt			= no
req_extensions		= v3_req

[ req_distinguished_name ]
C			= US
ST			= Pennsylvania
L			= Pittsburgh
O			= XIA Named Content
OU			= XIA Test Publisher
CN			= %(publisher)s
emailAddress		= test@xia.cs.cmu.edu

[ v3_req ]
basicConstraints = CA:FALSE
keyUsage = nonRepudiation, digitalSignature, keyEncipherment
"""
class Publisher:
    """ Named Content Publisher

    Publisher provides signatures for content that Clients can trust.
    This is achieved by having a Publisher certificate from a CA whose
    root certificate is trusted by the clients.

    1. The publisher starts with a keypair that it will use for signing content.
    2. These keys and a config file are used to generate a signing request.
    3. The signing request is presented to a CA and a certificate is obtained.
    4. The certificate must be published for Clients to retrieve when needed.
    5. Publisher can now use its private key to sign content Clients can trust.

    """

    def __init__(self, name):
        """ Create a keypair for Publisher if one doesn't exist """

        self.name = name

        # A directory for storing all Publishers' keys
        self.keydir = os.path.join(xiapyutils.xia_srcdir(), 'publishers')
        self.check_mkdir(self.keydir)

        # Each Publisher has its own working directory
        self.keydir = os.path.join(self.keydir, name)
        self.check_mkdir(self.keydir)

        # Create a directory to hold Publisher keys and signed certs
        print "Keys and certs will be stored in {}".format(self.keydir)

        self.keyfile = os.path.join(self.keydir, name + '.key')
        self.pubkeyfile = os.path.join(self.keydir, name + '.pub')
        self.privkeyfile = os.path.join(self.keydir, name + '.priv')
        self.reqfile = os.path.join(self.keydir, name + '.req')
        self.conffile = os.path.join(self.keydir, name + '.conf')

        # Now create the Publisher keys if they don't exist already
        if not os.path.exists(self.keyfile):
            print "Creating new key pair for the Publisher:", self.name
            self.make_new_key(self.keyfile)

        # Build the config for the signing request
        self.make_conffile()

    def check_mkdir(self, path):
        """ Create a directory if it doesn't exist already """
        if not os.path.exists(path):
            os.mkdir(path)
            os.chmod(path, 0700)

    def make_new_key(self, keyfile, algo="des3", size=1024):
        """ Create a new key pair for signing and certificates """
        cmd = "openssl genrsa -{} -out {} {}".format(algo, keyfile, size)
        check_call(cmd.split())

    def make_conffile(self):
        """ A config file for the signing request """
        config = open(self.conffile, 'w')
        config.write(REQ_TEMPLATE % {'publisher': self.name})

    def make_signing_request(self):
        """ Build a request that will be signed by a CA """
        self.make_conffile()
        cmd = "openssl req -new -key {} -out {} -config {}".format(
                self.keyfile, self.reqfile, self.conffile)
        check_call(cmd.split())
        print "Request ready for CA:", self.reqfile
        return self.reqfile

    def make_public_key(self):
        """ Output public key in RSAPublicKey format """
        cmd = "openssl rsa -pubout -in {} -out {}".format(
                self.keyfile, self.pubkeyfile)
        check_call(cmd.split())
        print "Public key stored in:", self.pubkeyfile
        return self.pubkeyfile

    def make_private_key(self):
        """ Output private key in a format usable by XIA """
        cmd = "openssl rsa -in {} -outform PEM -out {}".format(
                self.keyfile, self.privkeyfile)
        check_call(cmd.split())
        print "Private key stored in:", self.privkeyfile
        return self.privkeyfile

def parse_args():
    parser = argparse.ArgumentParser(description="XIA Named Content Publisher")
    parser.add_argument("-n", "--name",
            help="publisher name", type=str, required=True)
    parser.add_argument("-p", "--pubkey",
            help="generate public key", action="store_true")
    parser.add_argument("-k", "--privkey",
            help="generate private key in PEM for XIA use", action="store_true")
    parser.add_argument("-r", "--request_sign",
            help="make a signing request to send to CA", action="store_true")
    args =  parser.parse_args()
    return args

if __name__ == "__main__":
    args = parse_args()
    publisher = Publisher(args.name)
    if args.request_sign:
        req_file = publisher.make_signing_request()
    if args.pubkey:
        pub_key = publisher.make_public_key()
    if args.privkey:
        priv_key = publisher.make_private_key()

