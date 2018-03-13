#!/usr/bin/env python2.7
#

import re
import os.path
import xiapyutils

class AddrConf:
    '''Interface to etc/address.conf configuration file

    The etc/address.conf file contains several important configuration
    settings for the XIA instance. This class provides a common interface
    for all python scripts to read and process the file
    '''

    _addrconfname = 'etc/address.conf'
    _addrconfpattern = re.compile('^(\w+)\s+(\w+)\s+\((.+)\)')

    # Set up read access to the address.conf file
    def __init__(self):
        srcdir = xiapyutils.xia_srcdir()
        self._addrconfpath = os.path.join(srcdir, self._addrconfname)
        if not os.path.exists(self._addrconfpath):
            raise FileNotFoundError(
                    errno.ENOENT, os.strerror(errno.ENOENT), self._addrconfpath)
        self._hostname = None
        self._hosttype = None
        self._hid = None
        self._ad = None
        self._initialized = False
        # Remove if need to initialize on the fly
        self.initialize()

    # Read in common info from the address.conf
    def initialize(self):
        if self._initialized:
            return

        with open(self._addrconfpath) as addrconf:
            for line in addrconf:
                match = self._addrconfpattern.match(line)
                if match:
                    self._hostname = match.group(1)
                    self._hosttype = match.group(2)
                    self._hid = match.group(3)

                    if 'Router' in self._hosttype:
                        entries = self._hid.split(' ')
                        if len(entries) == 2:
                            self._ad, self._hid = entries
                # ASSUMPTION: Only one entry in address.conf
                self._initialized = True
                break

    def hostname(self):
        return self._hostname

    def hosttype(self):
        return self._hosttype

    def ad(self):
        return self._ad

    def hid(self):
        return self._hid

if __name__ == '__main__':
    addrconf = AddrConf()
    print 'Hostname:', addrconf.hostname()
    print 'Hosttype:', addrconf.hosttype()
    print 'AD at startup:', addrconf.ad()
    print 'HID at startup', addrconf.hid()
