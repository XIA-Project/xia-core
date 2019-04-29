# XIA network configuration service

# Starts with a config file with format:
#  [<router_name>]
#     ControlAddress = <dot_separated_ip_addr>
#     HostInterface  = <interface_to_which_AID_hosts_connect>
#     Peer_<n>       = <peer_name>:<iface>
# where, <n> lets the router have a number of peers
#

import os
import sys

from twisted.internet import reactor
from twisted.internet.protocol import Protocol, ClientFactory
from twisted.protocols.basic import Int32StringReceiver

# Bring in xia and overlay tools into path
srcdir = os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]
sys.path.append(os.path.join(srcdir, 'bin'))
sys.path.append(os.path.join(srcdir, 'tools/overlay'))

import xiapyutils

import xiaconfigdefs
import configrequest_pb2

from xiaconfigreader import XIAConfigReader

HELPER_PORT = 9854

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

class ConfigClient(Int32StringReceiver):
    def __init__(self, router, addr):
        self.router = router
        self.server_addr = addr
        self.initialized = False

    def stringReceived(self, data):
        if not self.initialized:
            # Get greeting from server and send a request for interfaces
            if data == "Helper Ready":
                self.initialized = True
                request = configrequest_pb2.Request()
                request.type = configrequest_pb2.Request.IFACE_INFO
                serialized_request = request.SerializeToString()
                self.sendString(serialized_request)
            else:
                print "ERROR: invalid greeting from server"
        else:
            # Got a response from server for one of our requests
            response = configrequest_pb2.Request()
            response.ParseFromString(data)
            if response.type == configrequest_pb2.Request.IFACE_INFO:
                self.handleInterfaceInfoResponse(response)
            elif response.type == configrequest_pb2.Request.ROUTER_CONF:
                self.handleRouterConfResponse(response)
            else:
                print "ERROR: invalid response from server"

    def handleInterfaceInfoResponse(self, response):
        print "Got interface info for:", self.router
        print "which is at address:", self.server_addr
        if response.type != configrequest_pb2.Request.IFACE_INFO:
            print "ERROR: Invalid response to iface_info request"
            return

        print "Router:", self.router
        r_click = RouterClick(self.router)
        for iface in response.ifrequest.interfaces:
            r_click.add_interface(iface.name, iface.ipaddr, iface.macaddr)
        request = configrequest_pb2.Request()
        request.type = configrequest_pb2.Request.ROUTER_CONF
        request.routerconf.configfile = r_click.to_string()
        self.sendString(request.SerializeToString())

    def handleRouterConfResponse(self, response):
        if response.type != configrequest_pb2.Request.ROUTER_CONF:
            print "ERROR: Invalid router config response"
            return
        print "Got valid response to router config request"


class ConfigClientFactory(ClientFactory):
    # def startedConnecting(self, connector)
    # def clientConnectionLost(self, connector, reason)
    # def clientConnectionFailed(self, connector, reason)
    def __init__(self, router):
        self.router = router

    def buildProtocol(self, addr):
        print "Connected to:", addr
        return ConfigClient(self.router, addr)

class XIAConfigurator:
    def __init__(self, config):
        self.config = config
        print 'Here are the routers we know of'
        for router in self.config.routers():
            print router
            print 'control_addr:', self.config.control_addr(router)
            print 'host_iface:', self.config.host_iface(router)

    def configure(self):
        for router in self.config.routers():
            if router != 'r1':
                continue
            reactor.connectTCP(self.config.control_addr(router),
                    xiaconfigdefs.HELPER_PORT,
                    ConfigClientFactory(router))
        reactor.run()

if __name__ == "__main__":
    conf_file = os.path.join(xiapyutils.xia_srcdir(), 'tools/overlay/demo.conf')
    config = XIAConfigReader(conf_file)
    configurator = XIAConfigurator(config)
    configurator.configure()
