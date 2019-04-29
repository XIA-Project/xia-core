import os
import sys

# Install python-twisted package for this functionality
from twisted.internet.protocol import Protocol, Factory
from twisted.protocols.basic import Int32StringReceiver
from twisted.internet import reactor

# Bring in xia and overlay tools into path
srcdir = os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]
sys.path.append(os.path.join(srcdir, 'bin'))
sys.path.append(os.path.join(srcdir, 'tools/overlay'))

import xiapyutils

import configrequest_pb2
import xiaconfigdefs
from xiaconfigdefs import ROUTER_CLICK

import interfaces

class Helper(Int32StringReceiver):
    def __init__(self, common_data, addr):
        self.common_data = common_data
        self.addr = addr

    def connectionMade(self):
        self.sendString(xiaconfigdefs.HELPER_GREETING);

    def connectionLost(self, reason):
        # Clean up any state created in common_data for this connection
        print "Connection to client lost"

    def handleInterfaceRequest(self, request):
        print "Got interface request"

        # Fill in interface information
        for iface in interfaces.get_interfaces():
            if_info = request.ifrequest.interfaces.add()
            if_info.name = iface
            if_info.ipaddr = interfaces.get_ip_addr(iface)
            if_info.macaddr = interfaces.get_mac_addr(iface)

        # Send the if_request back to the requestor
        self.sendString(request.SerializeToString())

    def handleRouterConfRequest(self, request):
        conf_file = os.path.join(xiapyutils.xia_srcdir(), ROUTER_CLICK)
        with open(conf_file, 'w') as config:
            config.write(request.routerconf.configfile)
        #print "Got router.click file:\n-----------------------"
        #print request.routerconf.configfile
        #print "---------------------------"
        response = configrequest_pb2.Request()
        response.type = configrequest_pb2.Request.ROUTER_CONF
        self.sendString(response.SerializeToString())

    def handleRoutesRequest(self, request):
        print "Got IP routes to configure"

    def handleConfigRequest(self, request):
        if request.type == configrequest_pb2.Request.IFACE_INFO:
            self.handleInterfaceRequest(request)
        elif request.type == configrequest_pb2.Request.ROUTER_CONF:
            self.handleRouterConfRequest(request)
        elif request.type == configrequest_pb2.Request.IP_ROUTES:
            self.handleRoutesRequest(request)
        else:
            print "ERROR: Unknown config request"

    def stringReceived(self, request):
        try:
            # Convert request into protobuf and handle the request
            conf_request = configrequest_pb2.Request()
            conf_request.ParseFromString(request)
            self.handleConfigRequest(conf_request);
        except:
            print "ERROR: invalid data instead of request"
            self.transport.loseConnection();

class HelperFactory(Factory):

    def __init__(self):
        self.common_data = {}

    def buildProtocol(self, addr):
        return Helper(self.common_data, addr)

# Run helper service
reactor.listenTCP(xiaconfigdefs.HELPER_PORT, HelperFactory())
reactor.run()
