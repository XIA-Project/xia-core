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
from twisted.internet.endpoints import TCP4ClientEndpoint, connectProtocol

# Bring in xia and overlay tools into path
srcdir = os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]
sys.path.append(os.path.join(srcdir, 'bin'))
sys.path.append(os.path.join(srcdir, 'tools/overlay'))

import xiapyutils

import xiaconfigdefs
import configrequest_pb2

from xiaconfigreader import XIAConfigReader
from routerclick import RouterClick

class ConfigClient(Int32StringReceiver):
    def __init__(self, router, configurator):
        self.router = router
        self.configurator = configurator
        self.initialized = False

    def connectionLost(self, reason):
        self.configurator.protocol_instances.remove(self)
        if len(self.configurator.protocol_instances) == 0:
            reactor.stop()

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
                self.transport.loseConnection()
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
                self.transport.loseConnection()

    def handleInterfaceInfoResponse(self, response):
        print "Got interface info for:", self.router
        if response.type != configrequest_pb2.Request.IFACE_INFO:
            print "ERROR: Invalid response to iface_info request"
            self.transport.loseConnection()
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
            self.transport.loseConnection()
            return
        print "Got valid response to router config request"
        # End the connection because the interaction is complete
        self.transport.loseConnection()


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
        self.protocol_instances = []
        print 'Here are the routers we know of'
        for router in self.config.routers():
            print router
            print 'control_addr:', self.config.control_addr(router)
            print 'host_iface:', self.config.host_iface(router)

    def gotProtocol(self, protocol):
        self.protocol_instances.append(protocol)

    def configure(self):
        for router in self.config.routers():
            # Endpoint for client connection
            endpoint = TCP4ClientEndpoint(reactor,
                    self.config.control_addr(router),
                    xiaconfigdefs.HELPER_PORT)
            # Link ConfigClient protocol instance to endpoint
            d = connectProtocol(endpoint, ConfigClient(router, self))
            d.addCallback(self.gotProtocol)
        reactor.run()

if __name__ == "__main__":
    conf_file = os.path.join(xiapyutils.xia_srcdir(), 'tools/overlay/demo.conf')
    config = XIAConfigReader(conf_file)
    configurator = XIAConfigurator(config)
    configurator.configure()
