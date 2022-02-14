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
from twisted.internet import defer
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
import clientconfig_pb2

from xiaconfigreader import XIAConfigReader
from xiaclientconfigurator import XIAClientConfigurator
from routerclick import RouterClick

import time

class ConfigRouter(Int32StringReceiver):
    def __init__(self, router, configurator, xid_wait):

        self.router = router
        self.configurator = configurator
        self.initialized = False
        self.xid_wait = xid_wait

    def connectionLost(self, reason):
        print "connection lost with client"
        # self.configurator.protocol_instances.remove(self)
        # if len(self.configurator.protocol_instances) == 0:
        #     time.sleep(50)
            # print "Going to configure clients now"
            # configure client after last router is configured
            # clientConfigurator = XIAClientConfigurator(self.configurator)
            # clientConfigurator.configureClient()
            #reactor.stop()
        
    def stringReceived(self, data):
        if not self.initialized:
            # Get greeting from server and send a request for interfaces
            if data == "Helper Ready":
                self.initialized = True
                request = configrequest_pb2.Request()
                request.type = configrequest_pb2.Request.IFACE_INFO
                config = self.configurator.config
                for iface in config.router_ifaces[self.router]:
                    if_info = request.ifrequest.interfaces.add()
                    if_info.name = iface
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
            elif response.type == configrequest_pb2.Request.START_XIA:
                self.handleStartXIAResponse(response)
            elif response.type == configrequest_pb2.Request.GATHER_XIDS:
                self.handleGatherXIDsResponse(response)
            elif response.type == configrequest_pb2.Request.IP_ROUTES:
                self.handleIPRoutesResponse(response)
            elif response.type == configrequest_pb2.Request.START_XCACHE:
                self.handleStartXcacheResponse(response)
            else:
                print "ERROR: invalid response from server"
                self.transport.loseConnection()

    def handleInterfaceInfoResponse(self, response):
        # Got interface information
        print "Got interface info for:", self.router
        if response.type != configrequest_pb2.Request.IFACE_INFO:
            print "ERROR: Invalid response to iface_info request"
            self.transport.loseConnection()
            return

        # Build a router.click for the router
        # and save IP addr in configurator.iface_addrs
        r_click = RouterClick(self.router)
        for iface in response.ifrequest.interfaces:
            ifname, ipaddr, macaddr = iface.name, iface.ipaddr, iface.macaddr
            r_click.add_interface(ifname, ipaddr, macaddr)
            self.configurator.iface_addrs[(self.router, ifname)] = ipaddr

        # Send it over to be deployed on the router
        request = configrequest_pb2.Request()
        request.type = configrequest_pb2.Request.ROUTER_CONF
        request.routerconf.configfile = r_click.to_string()
        self.sendString(request.SerializeToString())

    def waitForResolvConf(self):
        if len(self.configurator.resolvconf) == 0:
            reactor.callLater(0.1, self.waitForResolvConf)
        else:
            # Send a request to start XIA on router
            request = configrequest_pb2.Request()
            request.type = configrequest_pb2.Request.START_XIA
            request.startxia.command = './bin/xianet -r -l6 restart'
            request.startxia.resolvconf = self.configurator.resolvconf
            self.sendString(request.SerializeToString())

    def handleRouterConfResponse(self, response):
        # The router.click was successfully deployed
        if response.type != configrequest_pb2.Request.ROUTER_CONF:
            print "ERROR: Invalid router config response"
            self.transport.loseConnection()
            return
        print "Got valid response to router config request"
        # Start the router if it is the nameserver
        if self.configurator.nameserver == self.router:
            request = configrequest_pb2.Request()
            request.type = configrequest_pb2.Request.START_XIA
            request.startxia.command = './bin/xianet -rn -l6 restart'
            self.sendString(request.SerializeToString())
        else:
            # Otherwise wait for resolv.conf to arrive from nameserver
            if len(self.configurator.resolvconf) == 0:
                self.waitForResolvConf()

    # A response with resolv.conf means this router was a nameserver
    # Saving the resolv.conf into XIAConfigurator triggers new
    #    commands to start XIA on other routers with that file
    def handleStartXIAResponse(self, response):
        # Will have resolvconf field if nameserver started
        if response.type != configrequest_pb2.Request.START_XIA:
            print "ERROR: Invalid start XIA response"
            self.transport.loseConnection()
            return
        print "XIA started on", self.router
        if len(response.startxia.resolvconf) > 0:
            print "resolv.conf:"
            print response.startxia.resolvconf
            # Save off resolv.conf for other connections to send
            self.configurator.resolvconf = response.startxia.resolvconf

        # Now, ask the router to provide its AD, HID
        request = configrequest_pb2.Request()
        request.type = configrequest_pb2.Request.GATHER_XIDS
        self.sendString(request.SerializeToString())

    def sendIPRoutesRequest(self):
        request = configrequest_pb2.Request()
        request.type = configrequest_pb2.Request.IP_ROUTES

        # Routes to set up from this router to others
        routes = self.configurator.config.route_info[self.router]
        for route in routes:

            # Destination router, our interface, nexthop iface, nexthop name
            dest, our_iface, next_iface, next_name = route
            print("Adding route : ")
            print(route)

            # Convert dest router name to corresponding AD
            dest_ad, dest_hid = self.configurator.xids[dest]

            # Convert our_iface to corresponding outgoing port number
            our_ifaces = self.configurator.config.router_ifaces[self.router]
            port = our_ifaces.index(our_iface)

            # Their IP addr (from configurator.iface_addrs
            ipaddr = self.configurator.iface_addrs[next_name, next_iface]
            next_port = 8770 # Should be fixed or get from iface

            # Add a new route request
            print("xiaconfigurator : set up route to destAD router");
            route = "./bin/xroute -a AD,{},{},{}:{}".format(
                    dest_ad, port, ipaddr, next_port)
            request.routes.route_cmds.append(route)

            print("xiaconfigurator : set up route to destSID");
            dest_sid = self.configurator.config.sid[dest]
            sid_route = "./bin/xroute -a SID,{},{},{}:{},{}".format(
                    dest_sid, port, ipaddr, 8772, 7)
            print("Sending %s" %sid_route)
            request.routes.route_cmds.append(sid_route)
        self.sendString(request.SerializeToString())


    def sendRoutesRequest(self, event):
        request = configrequest_pb2.Request()
        request.type = configrequest_pb2.Request.IP_ROUTES

        add_links = []
        remove_links = []

        try:
            add_links = self.configurator.events[event]['add_links'][self.router]
            print("Add links")
            print add_links
            link_info = self.configurator.config.link_info[self.router]
            for other_router in add_links:
                dest_ad, dest_hid = self.configurator.xids[other_router]
                my_iface, other_iface = link_info[other_router]

                # Convert our_iface to corresponding outgoing port number
                my_ifaces = self.configurator.config.router_ifaces[self.router]
                port = my_ifaces.index(my_iface)

                # Their IP addr (from configurator.iface_addrs
                ipaddr = self.configurator.iface_addrs[other_router, other_iface]
                next_port = 8770 # Should be fixed or get from iface

                # Add a new route request
                route = "./bin/xroute -a AD,{},{},{}:{}".format(
                        dest_ad, port, ipaddr, next_port)
                request.routes.route_cmds.append(route)
                print("Sending %s" %route)
                dest_sid = self.configurator.config.sid[other_router]
                sid_route = "./bin/xroute -a SID,{},{},{}:{},{}".format(
                        dest_sid, port, ipaddr, 8772, 7)
                print("Sending %s" %sid_route)
                request.routes.route_cmds.append(sid_route)

        except KeyError:
            pass

        try:
            remove_links = self.configurator.events[event]['remove_links'][self.router]
            print("remove links")
            print remove_links

            link_info = self.configurator.config.link_info[self.router]
            for other_router in remove_links:
                dest_sid = self.configurator.config.sid[other_router]
                sid_route = "./bin/xroute -r SID,{}".format(dest_sid)
                print("Sending %s" %sid_route)
                request.routes.route_cmds.append(sid_route)

        except KeyError:
            pass       

        if len(add_links) > 0 or len(remove_links) > 0:
            self.sendString(request.SerializeToString())

    def handleIPRoutesResponse(self, response):
        if response.type != configrequest_pb2.Request.IP_ROUTES:
            print "ERROR: Invalid IP route config response"
            self.transport.loseConnection()
            return
        print "IP Routes configured on", self.router

        # Now, request Xcache start if it was requested by user
        if self.configurator.config.xcache[self.router] == True:
            print "Check xCache enabled on from Configurator ", self.router
            request = configrequest_pb2.Request()
            request.type = configrequest_pb2.Request.START_XCACHE
            cmd = "./build/xcache &"
            request.startxcache.command = cmd;
            request.startxcache.hostname = self.router
            self.sendString(request.SerializeToString())
            # handleStartXcacheResponse will be called when xcache has started
            # return

            # print "Back after configureClient"


        # # Otherwise end the connection because the interaction is complete
        # self.transport.loseConnection()

        if not self.configurator.routers_setup:
            self.configurator.first_route_req_count -= 1
            # first event done, setup clients
            if self.configurator.first_route_req_count == 0:
                self.configurator.routers_setup = True
                time.sleep(100)
                self.configurator.configureClients()


    def handleStartXcacheResponse(self, response):
        if response.type != configrequest_pb2.Request.START_XCACHE:
            print "ERROR: Failed to start Xcache on", self.router
            self.transport.loseConnection()
            return
        if response.startxcache.result != True:
            print "ERROR: Failed to start Xcache on:", self.router
        else:
            print "Started Xcache on", self.router
        self.transport.loseConnection()

    def handleGatherXIDsResponse(self, response):

        if response.type != configrequest_pb2.Request.GATHER_XIDS:
            print "ERROR: Invalid gather XIDs response"
            self.transport.loseConnection()
            return
        (ad, hid) = response.gatherxids.ad, response.gatherxids.hid
        print self.router, ad, hid
        self.configurator.xids[self.router] = (ad, hid)
        self.xid_wait.callback(self)


class XIAConfigurator:
    def __init__(self, config):
        self.config = config
        self.protocol_instances = []
        self.nameserver = self.config.nameserver
        self.resolvconf = ""
        self.xids = {} # router: (ad, hid)
        self.iface_addrs = {} # (router, iface_name) : ipaddr
        self.first_route_req_count = 0
        self.events = self.config.events
        self.routers_setup = False

        print self.nameserver, 'is the nameserver'
        print 'Here are the routers we know of'
        for router in self.config.routers():
            print router
            print 'control_addr:', self.config.control_addr(router)
            print 'host_iface:', self.config.host_iface(router)

    # All routers sent in their AD, HID. Send IP routes to all routers
    def sendIPRoutes(self, result):

        for (success, protocol) in result:
            if success:
                print "Sending IP routes to {}".format(protocol.router)
                protocol.sendIPRoutesRequest()
            else:
                print "ERROR: failure sending IP routes for a protocol"

    # All routers sent in their AD, HID. Send IP routes to all routers
    def sendRoutes(self, event, result):
        for (success, protocol) in result:
            if success:
                print "Sending IP routes to {}".format(protocol.router)
                protocol.sendRoutesRequest(event)
            else:
                print "ERROR: failure sending IP routes for a protocol"


    def runEvents(self, result):
        print "Starting events"
        for i, (event, val) in enumerate(self.events.items()):
            print("Scheduling event %s" % event)
            delay = int(val['delay'])
            if i == 0:
                self.first_route_req_count = val['link_count']
            reactor.callLater(delay, self.sendRoutes, event, result)
        
    def configureClients(self):
        print "Going to configure client"
        clientConfigurator = XIAClientConfigurator(self)
        clientConfigurator.configureClient()

    def gotProtocol(self, protocol):
        self.protocol_instances.append(protocol)

    def configure(self):

        xid_waiters = []

        for router in self.config.routers():
            # Endpoint for client connection

            endpoint = TCP4ClientEndpoint(reactor,
                    self.config.control_addr(router),
                    xiaconfigdefs.HELPER_PORT)

            # Each connection will wait for all others to gather XIDs
            xid_wait = defer.Deferred()
            xid_waiters.append(xid_wait)

            # Link ConfigRouter protocol instance to endpoint
            d = connectProtocol(endpoint, ConfigRouter(router, self, xid_wait))
            d.addCallback(self.gotProtocol)
    

        # Callback called, when all routers have collected their XIDs
        dl = defer.DeferredList(xid_waiters, consumeErrors=True)
        # dl.addCallback(self.sendIPRoutes)
        dl.addCallback(self.runEvents)

        reactor.run()


if __name__ == "__main__":
    conf_file = os.path.join(xiapyutils.xia_srcdir(), 'tools/overlay/demo.conf')
    events_file = os.path.join(xiapyutils.xia_srcdir(), 'tools/overlay/events.conf')
    config = XIAConfigReader(conf_file, events_file)
    configurator = XIAConfigurator(config)
    configurator.configure()
