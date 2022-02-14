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
import clientconfig_pb2

from ConfigParser import RawConfigParser

mobility_time = 5

class XIAClientConfigReader:
    def __init__(self, config_filename):
       self.routers = {}
       self.default_router = {}
       self.control_addr = {}
       self.control_port = {}
       self.aid = {}
       self.ad = {}
       self.hid = {}
       self.router_addr = {}
       self.router_iface = {}
       self.serverdag = {}
       self.mobile = {}

       # Read in the config file
       parser = RawConfigParser()
       parser.read(config_filename)

       # Router names are the section names in the config file
       clients = parser.sections()
       if len(clients) == 0:
          print "ERROR: No sections found in config file"

       # Read in info into our internal data structures
       for client in clients:

           # Interface names for each router (comma separated list)
           routers = parser.get(client, 'Routers')
           routers = routers.replace(' ', '')
           router_list = routers.split(',')
           self.routers[client] = router_list

           iface = parser.get(client, 'Interfaces')
           iface = iface.replace(' ', '')
           ifaces = iface.split(',')
           self.router_iface[client] = {}
           r_iface = {}
           for i in range(len(ifaces)):
              r_iface[router_list[i]] = ifaces[i]
           self.router_iface[client] = r_iface

           self.default_router[client] = parser.get(client, 'Default')
           self.control_addr[client] = parser.get(client, 'ControlAddress')
           self.control_port[client] = parser.get(client, 'ControlPort')
           self.serverdag[client] = parser.get(client, 'ServerDag')
           self.aid[client] = parser.get(client, 'AID')
           self.mobile[client] = False
           try:
               if parser.getboolean(client, "Mobile"):
                   self.mobile[client] = True
           except:
               pass

    def clients(self):
        return self.routers.keys()

class ConfigClient(Int32StringReceiver):
    def __init__(self, client, clientConfigurator):
        self.client = client
        self.clientConfigurator = clientConfigurator
        self.connected_clients = 0

    def connectionLost(self, reason):
        #self.connected_clients.remove(self)
        print "Lost connection with " + self.client
        self.connected_clients -= 1
        # if self.clientConfigurator.connected_clients == 0:
        #     print "!!!!!!!!!!! Stopping the configurator  !!!!!!!!!!!!!"
            #reactor.stop()

    def connectionMade(self):
        # configure with default router
        self.connected_clients += 1
        print "Step3. Check connect to client"
        self.sendConfig(self.clientConfigurator.clientConfig.default_router[self.client])

        if self.clientConfigurator.clientConfig.mobile[self.client] == True:
          print "Making " + self.client + "mobile"
          # We don't want to recursively do this, disable mobility
          self.clientConfigurator.clientConfig.mobile[self.client] = False
          for router in self.clientConfigurator.clientConfig.routers[self.client]:
            print "\n Checking out router " + router + "\n" 
            if router != self.clientConfigurator.clientConfig.default_router[self.client]:
              print "Adding a call for " + router
              reactor.callLater(mobility_time, self.mobilityConfig, self.client, router)
    
    def sendConfig(self, router):
        response = clientconfig_pb2.Config()
        print "-----------------------------------"
        print "Step4. Sending config to " + self.client
        print "-----------------------------------"
        response.name = self.client
        response.ipaddr = self.clientConfigurator.clientConfig.router_addr[router]
        response.iface = self.clientConfigurator.clientConfig.router_iface[self.client][router]
        response.port = "8770"
        response.AID = self.clientConfigurator.clientConfig.aid[self.client]
        response.AD = self.clientConfigurator.clientConfig.ad[router]
        response.HID =self.clientConfigurator.clientConfig.hid[router]
        response.serverdag = self.clientConfigurator.clientConfig.serverdag[self.client]
        
        print "serverdag updated with server's default route AD and HID", response.serverdag, "\n"
        self.sendString(response.SerializeToString())
        print response.SerializeToString()
        print "-----------------------------------"


    def mobilityConfig(self, client, router):
        # new default router
        self.clientConfigurator.clientConfig.default_router[client] = router
        endpoint = TCP4ClientEndpoint(reactor, self.clientConfigurator.clientConfig.control_addr[client],
                                          int(self.clientConfigurator.clientConfig.control_port[client]))

        d = connectProtocol(endpoint, ConfigClient(client, self.clientConfigurator))


class XIAClientConfigurator():
    def __init__(self, configurator):
	self.connected_clients = 0
        
	clientConfig = XIAClientConfigReader('tools/overlay/client.conf')
        updated_DAG =""
        for client in clientConfig.clients():
            print "Step2. Check from clientconfig init"
            print client + ':'
            for router in clientConfig.routers[client]:
                clientConfig.ad[router] = configurator.xids[router][0]
                clientConfig.hid[router] = configurator.xids[router][1]
                clientConfig.router_addr[router] = configurator.config.host_ipaddrs[router]
                
                #locate DAG from the server endpoint
                if clientConfig.serverdag[client].find(clientConfig.aid[client]) != -1:
                    updated_DAG = "RE " + clientConfig.ad[router] + " " + clientConfig.hid[router]+ " " + clientConfig.aid[client]
                print configurator.xids[router]

        #update Serverdag in client.conf to the DAG from default router connected to server endpoint
        config_clientfile = RawConfigParser()
        config_clientfile.read('tools/overlay/client.conf')
        for section in config_clientfile.sections():
            config_clientfile.set(section, "ServerDag", updated_DAG)
        with open('tools/overlay/client.conf', 'w+') as cf:
            config_clientfile.write(cf)

        #upload newDAG to clientConfig for all endpoints
        ClientConfig_u = XIAClientConfigReader('tools/overlay/client.conf')
        for client in clientConfig.clients():
            clientConfig.serverdag[client] = updated_DAG

        self.clientConfig = clientConfig

    def configureClient(self):
        for client in self.clientConfig.clients():
            endpoint = TCP4ClientEndpoint(reactor, 
                                          self.clientConfig.control_addr[client],
                                          int(self.clientConfig.control_port[client]))

            d = connectProtocol(endpoint, ConfigClient(client, self))
            #d.addCallback(self.addClient)

            #reactor.run()

    def addClient(self):
        self.connected_clients += 1
