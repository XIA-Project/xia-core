#!/usr/bin/python
#
# Copyright 2011-2017 Carnegie Mellon University
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

import os
import xiapyutils
from configparser import ConfigParser

xkeys = ['hostname', 'nodetype', 'numports', 'hid', 'cache', 'broker', 'publisher', 'manifest', 'client']
ckeys = ['ad', 'controller_sid', 'nameserver_sid', 'rendezvous_sid', 'rendezvous_ctl_sid', 'controller_dag']
wkeys = [ 'mac', 'addr', 'port']

class nodeconf:
    ''' XIA Configuration Info Class '''

    def __init__(self):
        self.conf = ConfigParser()
        self.fname = os.path.join(xiapyutils.xia_srcdir(), 'etc/xia.conf')
#        self.hostname = xiapyutils.getxiaclickhostname()


    def read(self):
        if os.path.isfile(self.fname):
            self.conf.read(self.fname)
            return True
        else:
            return False


    def write(self):
        with open(self.fname, 'wb') as configfile:
            self.conf.write(configfile)


    def reset(self):
        self.conf.remove_section('xia')
        self.conf.remove_section('controller')
        self.conf.remove_section('ignore')
        self.conf.remove_section('waveserver')


    def _get(self, section, key, default):
        try:
            return self.conf.get(section, key)
        except:
            return default


    def get(self, key, default):
        if key in xkeys:
            return self._get('xia', key, default)
        elif key in ckeys:
            if self.nodetype() == 'controller':
                return self._get('controller', key, default)
            else:
                return None
        elif key in wkeys:
            return self._get('waveserver', key, default)
        elif key == 'ignore':
            return interfaces()
        else:
            return default


    def _set(self, section, key, value):
        if not self.conf.has_section(section):
            self.conf.add_section(section)
        self.conf.set(section, key, value)


    def set(self, key, value):
        if key in xkeys:
            return self._set('xia', key, value)
        elif key in ckeys:
            if self.nodetype() == 'controller':
                return self._set('controller', key, value)
            else:
                return False
        elif key in wkeys:
            return self._set('waveserver', key, value)
        elif key == 'ignore':
            return self._set('ignore', value, True)
        else:
            return False
        return True

    def _common_bool(self, name, flag):
        enabled = self._get('xia', name, flag)
        return enabled.lower() in ['true', '1', 'yes', 'on']

    def ad(self):
        return self._get('controller', 'ad', None)

    def hid(self):
        return self._get('xia', 'hid', None)

    def controller_sid(self):
        return self._get('controller', 'controller_sid', None)

    def controller_dag(self):
        return self._get('controller', 'controller_dag', None)

    def nameserver_sid(self):
        return self._get('controller', 'nameserver_sid', None)

    def nodetype(self):
        return self._get('xia', 'nodetype', None)

    def cache(self):
        return self._common_bool('cache', 'True')

    def client(self):
        return self._common_bool('client', 'False')

    def broker(self):
        return self._common_bool('broker', 'False')

    def manifest(self):
        return self._common_bool('manifest', 'False')

    def hostname(self):
        return self._get('xia', 'hostname', 'xia')

    def nameserver(self):
        return self._get('controller', 'nameserver', 'SID:1110000000000000000000000000000000001113')

    def rendezvous_sid(self):
        return self._get('controller', 'rendezvous_sid', None)

    def rendezvous_ctl_sid(self):
        return self._get('controller', 'rendezvous_ctl_sid', None)

    def numports(self):
        return self._get('xia', 'numports', 4)

    def interfaces(self):
        list = []
        try:
            ifaces = self.conf.items('ignore')
            for (iface, ignored) in ifaces:
                if ignored:
                    list.append(iface)
        except:
            pass

        return list

    def waveserver(self):
        if self.conf.has_section('waveserver'):
            return (self.get('mac', None), self.get('addr', None), self.get('port', None))
        else:
            return None


    def dump(self):
        for section in self.conf.sections():
            print '%s"' % section
            for k,v in self.conf.items(section):
                print '  %s: %s' % (k, v)


if __name__ == "__main__":
    c = nodeconf()
    if c.read():
        c.dump()



