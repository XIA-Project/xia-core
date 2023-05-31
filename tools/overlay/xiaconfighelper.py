# Python standard libraries
import os
import sys
import subprocess
import inspect

# Install python-twisted package for this functionality
from twisted.internet.protocol import Protocol, Factory
from twisted.protocols.basic import Int32StringReceiver
from twisted.internet import reactor

# Bring in xia and overlay tools into path
srcdir = os.getcwd()[:os.getcwd().rindex('xia-core')+len('xia-core')]
sys.path.append(os.path.join(srcdir, 'bin'))
sys.path.append(os.path.join(srcdir, 'tools/overlay'))

# XIA libraries
import xiapyutils
from clickcontrol import ClickControl

# XIA Overlay configuration libraries and definitions
import configrequest_pb2
import xiaconfigdefs
from xiaconfigdefs import ROUTER_CLICK

import interfaces

picoquic_directory = "../picoquic"

class Helper(Int32StringReceiver):
    def __init__(self, common_data, addr):
        self.common_data = common_data
        self.addr = addr

    def connectionMade(self):
        self.sendString(xiaconfigdefs.HELPER_GREETING);

    def connectionLost(self, reason):    
        # Clean up any state created in common_data for this connection
        print "Connection to controller lost"

    def handleInterfaceRequest(self, request):
        # Fill in interface information
        for if_info in request.ifrequest.interfaces:
            if_info.ipaddr = interfaces.get_ip_addr(if_info.name)
            if_info.macaddr = interfaces.get_mac_addr(if_info.name)

        # Send the if_request back to the requestor
        self.sendString(request.SerializeToString())

    def handleRouterConfRequest(self, request):
        conf_file = os.path.join(xiapyutils.xia_srcdir(), ROUTER_CLICK)
        with open(conf_file, 'w') as config:
            config.write(request.routerconf.configfile)
        #print "Got router.click file:\n-----------------------"
        #print request.routerconf.configfile
        #print "---------------------------"
        print "Use the click config to create nodes from helper"
        node_conf_file = os.path.join(srcdir,'etc/nodes.conf')
        if not os.path.exists(node_conf_file):
            print "create nodes.conf from topology if not existing\n"
            subprocess.check_call([os.path.join(srcdir,'bin','read_topology'), conf_file])

        response = configrequest_pb2.Request()
        response.type = configrequest_pb2.Request.ROUTER_CONF
        self.sendString(response.SerializeToString())

    def handleRoutesRequest(self, request):
        print "Got IP routes to configure"
        for route_cmd in request.routes.route_cmds:
            print route_cmd
            subprocess.check_call(route_cmd, shell=True)
        request.routes.result = True
        self.sendString(request.SerializeToString())

    def getXcacheAID(self):
        xcache_conf_file = os.path.join(os.getcwd(),'conf/local.conf')
        with open(xcache_conf_file, 'r') as localconf:
            for line in localconf:
                if not line.startswith('XCACHE_AID'):
                    continue
                param, aid = line.strip().split('=')
                print "Xcache XID", aid
                return aid
        print "ERROR getting XcacheAID"
        print "must call from picoquic dir and have xcache.local.conf"
        return ''

    def registerXcache(self, hostname, aid):
        print "Confighelper: registering Xcache"
        with ClickControl() as click:
            return click.assignXcacheAID(hostname, aid)

    def handleStartXcacheRequest(self, request):
        print "Got request to start Xcache", request.startxcache.command
        cwd = os.getcwd()
        os.chdir(picoquic_directory)
        subprocess.check_call(request.startxcache.command, shell=True)
        aid = self.getXcacheAID()
        os.chdir(cwd)
        hostname = request.startxcache.hostname
        request.startxcache.result = self.registerXcache(hostname, aid)
        self.sendString(request.SerializeToString())

    # If the request came without a resolv.conf, this router is a nameserver
    # and a new resolv.conf file will be sent back in response
    #
    # Otherwise, this is a regular router that will be started with the
    # given resolv.conf file. Response will not have a resolv.conf
    def handleStartXIARequest(self, request):
        resolvconfreceived = False
        # If a resolv.conf was sent, write it to storage for router to use
        if len(request.startxia.resolvconf) > 0:
            resolvconffile = os.path.join(xiapyutils.xia_srcdir(),
                    'etc/resolv.conf')
            print 'Writing:', resolvconffile
            with open(resolvconffile, 'w') as resolvconf:
                resolvconf.write(request.startxia.resolvconf)
                resolvconfreceived = True

        print "Got request to restart XIA:", request.startxia.command

        stopcmd = request.startxia.command.replace("restart", "stop")
        startcmd = request.startxia.command.replace("restart", "start")

        print "Stopping XIA:", stopcmd
        subprocess.call(stopcmd, shell=True)

        print "Starting XIA:", startcmd
        subprocess.check_call(startcmd, shell=True)

        # We're done starting a non-nameserver router
        if resolvconfreceived:
            response = configrequest_pb2.Request()
            response.type = configrequest_pb2.Request.START_XIA
            self.sendString(response.SerializeToString())
            return

        # Send resolv.conf from the nameserver back for other routers to use
        conffile = os.path.join(xiapyutils.xia_srcdir(), 'etc/resolv.conf')
        with open(conffile, 'r') as resolvconf:
            request.startxia.resolvconf = resolvconf.read()
        self.sendString(request.SerializeToString())

    def handleGatherXIDsRequest(self, request):
        xdag_cmd = os.path.join(xiapyutils.xia_srcdir(), 'bin/xdag')
        xdag_out = subprocess.check_output(xdag_cmd, shell=True)
        router, re, ad, hid = xdag_out.split()
        request.gatherxids.ad = ad
        request.gatherxids.hid = hid
        self.sendString(request.SerializeToString())

    def handleConfigRequest(self, request):
        if request.type == configrequest_pb2.Request.IFACE_INFO:
            self.handleInterfaceRequest(request)
        elif request.type == configrequest_pb2.Request.ROUTER_CONF:
            self.handleRouterConfRequest(request)
        elif request.type == configrequest_pb2.Request.IP_ROUTES:
            self.handleRoutesRequest(request)
        elif request.type == configrequest_pb2.Request.START_XIA:
            self.handleStartXIARequest(request)
        elif request.type == configrequest_pb2.Request.GATHER_XIDS:
            self.handleGatherXIDsRequest(request)
        elif request.type == configrequest_pb2.Request.START_XCACHE:
            self.handleStartXcacheRequest(request)
        else:
            print "ERROR: Unknown config request"

    def stringReceived(self, request):
        #try:
            # Convert request into protobuf and handle the request
            conf_request = configrequest_pb2.Request()
            conf_request.ParseFromString(request)
            self.handleConfigRequest(conf_request);
        #except:
            #print "ERROR: invalid data instead of request"
            #self.transport.loseConnection();

class HelperFactory(Factory):
    def __init__(self):
        self.common_data = {}

    def buildProtocol(self, addr):
        return Helper(self.common_data, addr)

# Run helper service
reactor.listenTCP(xiaconfigdefs.HELPER_PORT, HelperFactory())
reactor.run()
