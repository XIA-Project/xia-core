import os
import sys

# Bring in xia and overlay tools into path
srcdir = os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]
sys.path.append(os.path.join(srcdir, 'bin'))
sys.path.append(os.path.join(srcdir, 'tools/overlay'))

import xiapyutils

import xiaconfigdefs
import configrequest_pb2

class RouterClick:
    def __init__(self, name):
        self.name = name
        self.interfaces = []

    def add_interface(self, iface_name, ipaddr, macaddr):
        self.interfaces.append((iface_name, ipaddr, macaddr))

    def to_string(self):
        rstr = """
require(library ../../click/conf/xia_router_lib.click);
require(library xia_address.click);

log::XLog(VERBOSE 0, LEVEL 6);

// router instantiation
"""
        rstr += "{} :: XIARouter4Port(1500, {}, 0.0.0.0".format(
                self.name, self.name)
        num_interfaces = len(self.interfaces)
        for index in range(4):
            if (index < num_interfaces):
                (iface_name, ipaddr, macaddr) =  self.interfaces[index]
                rstr += ", {}".format(macaddr)
            else:
                rstr += ", 00:00:00:00:00:00"
        rstr += ");\n"

        for index in range(4):
            if (index < num_interfaces):
                (iface_name, ipaddr, macaddr) = self.interfaces[index]
                rstr += '\nosock{}::XIAOverlaySocket("UDP", {}, {}, SNAPLEN 65536) -> [{}]{}[{}] -> osock{};\n'.format(index, ipaddr, 8770+index, index, self.name, index, index)
            else:
                rstr += '\nIdle -> [{}]{}[{}] -> Discard;\n'.format(
                        index, self.name, index)

        (iface_name, ipaddr, macaddr) = self.interfaces[0]
        rstr += '\nSocket("UDP", {}, 8769, SNAPLEN 65536) -> [4]{}\n'.format(
                ipaddr, self.name)
        rstr += '\nControlSocket(tcp, 7777);\n'
        return rstr
