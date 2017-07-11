#!/usr/bin/env python2.7
#

# Minimal Certifying Authority

import os
import sys

srcdir = os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]
sys.path.append(os.path.join(srcdir, "daemons/xcache/ca"))
sys.path.append(os.path.join(srcdir, "bin"))

import xiapyutils
from subprocess import check_call

class CA:
    """ Certifying Authority
    
    This is an implementation for a minimal CA that can produce a root
    certificate and sign certificates for given common names.

    We intend to use it primarily for named content, NCID, in XIA.

    On creation of an instance of CA:
    1. A new CA keypair is created if one doesn't exist in xia-core/ca
    2. A root CA certificate is created if one doesn't exist
    """
    keydir = os.path.join(xiapyutils.xia_srcdir(), 'ca')
    
    def __init__(self):
        """ Create a root certificate if one doesn't exist """
        print "Keys and certs will be stored in {}".format(CA.keydir)

        # Create a directory to hold CA keys and signed certs
        if not os.path.exists(CA.keydir):
            os.mkdir(CA.keydir)
            os.chmod(CA.keydir, 0700)

        keyfile = os.path.join(CA.keydir, 'ca.key')
        certfile = os.path.join(CA.keydir, 'ca.cert')
        new_key_created = False

        # Now create the CA keys if they don't exist already
        if not os.path.exists(keyfile):
            print "Creating new key pair for this CA"
            self.make_new_key(keyfile)
            new_key_created = True

        # Create a self-signed certificate. All clients must get this.
        if not os.path.exists(certfile) or new_key_created:
            print "Creating self-signed cert representing this CA"
            self.make_new_ss_cert(keyfile, certfile)
        print "Root Certificate:", certfile

        # Notify user that nCID support requires all clients to use ca.cert
        print "NOTE: Copy ca.cert to all clients supporting nCID"

    def make_new_key(self, keyfile, algo="des3", size=4096):
        """ Create a new key pair for signing and certificates """
        cmd = "openssl genrsa -{} -out {} {}".format(algo, keyfile, size)
        check_call(cmd.split())

    def make_new_ss_cert(self, keyfile, certfile, days=365):
        """ Create a root certificate """
        cmd = "openssl req -new -x509 -days {} -key {} -out {}".format(
                days, keyfile, certfile)
        check_call(cmd.split())


if __name__ == "__main__":
    ca = CA()
