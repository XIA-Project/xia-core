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

# XIA Overlay configuration libraries and definitions
import clientconfig_pb2

import interfaces

picoquic_directory = "../picoquic"

class Helper(Int32StringReceiver):
    def __init__(self, common_data, addr):
        print inspect.stack()[0][3]
        self.common_data = common_data
        self.addr = addr

    def connectionLost(self, reason):
        print inspect.stack()[0][3]
        # Clean up any state created in common_data for this connection
        print "Connection to client lost"

    def stringReceived(self, recvd_conf):
        print inspect.stack()[0][3]
        conf = clientconfig_pb2.Config()
        conf.ParseFromString(recvd_conf)

        self.handleConfig(conf)

    def handleConfig(self, config):
        print "----------------------------------------------"
    	print "Received"
    	print config
        print "----------------------------------------------"
        #reactor.stop()
        

class HelperFactory(Factory):

    def __init__(self):
        self.common_data = {}

    def buildProtocol(self, addr):
        return Helper(self.common_data, addr)

# Run helper service
reactor.listenTCP(8295, HelperFactory())
reactor.run()
