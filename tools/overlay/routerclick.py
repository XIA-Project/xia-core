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
                rstr += '\nosock{}::XIAOverlaySocket("UDP", {}, {}, SNAPLEN 65536) -> [{}]{}[{}] -> osock{};'.format(index, ipaddr, 8770, index, self.name, index, index)
            else:
                rstr += '\nIdle -> [{}]{}[{}] -> Discard;\n'.format(index, self.name, index)
        
        rstr += '\n rd :: XIAOverlayRouter();\n'
        for index in range(4):
            if (index < num_interfaces):
                (iface_name, ipaddr, macaddr) = self.interfaces[index]
                rstr += 'rsock{}::XIAOverlaySocket("UDP", {}, {}, SNAPLEN 65536) -> rd;\n'.format(index, ipaddr, 8772)

        rstr += '\n'
        for index in range(4):
            if (index < num_interfaces):
                rstr += 'rd[{}] -> rsock{};\n'.format(index, index)


        # rstr += '\n'
        # for index in range(4):
        #     if (index < num_interfaces):
        #         (iface_name, ipaddr, macaddr) = self.interfaces[index]
        #         rstr += 'rsock{}::XIAOverlaySocket("UDP", {}, {}, SNAPLEN 65536) -> rd{}::XIAOverlayRouted();\n'.format(index, ipaddr, 8772, index, index)
        
        # rstr += '\n'
        # for index in range(4):
        #     if (index < num_interfaces):
        #         for i in range(0,3):
        #             if (i < num_interfaces):
        #                 rstr += 'rd{}[{}] -> of{}::XIAOverlayFilter() -> rsock{};\n'.format(index, i, index*10+i, i)

        # rstr += '\n'
        # for index in range(4):
        #     if (index < num_interfaces):
        #         for i in range(0,3):
        #             if (i < num_interfaces):
        #                 rstr += 'of{}[{}] -> [{}]of{};\n'.format(index*10+i, 1, 1 , index*10+i)
        #                 rstr += 'of{}[{}] -> Discard; \n of{}[{}] -> Discard; \n'.format(index*10+i, 2, index*10+i, 3)



        # rstr += '\n'
        # (iface_name, ipaddr, macaddr) = self.interfaces[0]
        # rstr += '\nSocket("UDP", {}, 8769, SNAPLEN 65536) -> [4]{}\n'.format(
        #        ipaddr, self.name)
        rstr += '\nControlSocket(tcp, 7777);\n'
        return rstr
