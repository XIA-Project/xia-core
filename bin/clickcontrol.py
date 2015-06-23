#!/usr/bin/python
#
# Copyright 2013 Carnegie Mellon University
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

import sys
import socket

# Number of interfaces for each host type
numIfaces = {'XIAEndHost':4, 'XIARouter4Port':4, 'XIARouter2Port':2}

# Principal types
principals = ['AD', 'HID', 'SID', 'CID', 'IP']


class ClickControl:
    ''' Click controller
    Used to call write/read handlers inside click elements
    Specialized functions exist for assigning HID or Network DAG
    '''

    # Initialize a socket to talk to Click
    def __init__(self, clickhost='localhost', port=7777):
        self.initialized = False
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.sock.connect((clickhost, port))
            self.initialized = True
        except:
            print 'ERROR: Unable to connect to Click'
            sys.exit(-1)

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        if self.initialized:
            self.sock.close()

    # Send data to Click
    def send(self, command):
        commandlen = len(command)
        sent = self.sock.send(command)
        if sent != commandlen:
            return -1
        return 0

    # Receive data from Click
    def receive(self):
        return self.sock.recv(1024)

    # Call a write handler in Click
    def writeCommand(self, command):
        cmd = 'write %s\n' % command
        self.send(cmd)
        response = self.receive()
        if 'OK' not in response:
            print 'ERROR: %s: Resulted in: %s\n' % (cmd, response)
            return False
        return True

    # Get a list of elements' write handlers
    def getElements(self, hostname, hosttype, handler_name, iface_elem, elements):
        # Routing tables for each principal type
        for principal in principals:
            elements.append('xrc/n/proc/rt_%s' % principal)

        # Iterate over the number of interfaces for this host type
        for i in range(numIfaces[hosttype]):
            # Elements inside each interface card
            for elem in iface_elem:
                elements.append('xlc%d/%s' % (i, elem))

        # Prepend hostname to each hid_element
        return [hostname + '/' + e + '.' + handler_name for e in elements]

    # A list of all elements with HID write handler
    def hidElements(self, hostname, hosttype):
        # Elements in line card that need to be notified
        iface_elem = ['x', 'xarpq', 'xarpr', 'xchal', 'xresp']

        # Xtransport and XCMP elements in RouteEngine and RoutingCore
        hid_elem = ['xrc/xtransport', 'xrc/n/x', 'xrc/x']
        return getElements(self, hostname, hosttype, 'hid', iface_elem, hid_elem)

    # Assign an HID to a given host
    def assignHID(self, hostname, hosttype, hid):
        for element in self.hidElements(hostname, hosttype):
            if not self.writeCommand(element + ' ' + hid):
                return False
        return True

    # A list of all elements with Network DAG write handler
    def networkDagElements(self, hostname, hosttype):
        # Elements in line card that need to be notified
        iface_elem = ['x', 'xchal']

        # Xtransport and XCMP elements in RouteEngine and RoutingCore
        dag_elem = ['xrc/transport', 'xrc/n/x', 'xrc/x']
        return getElements(self, hostname, hosttype, 'network_dag', iface_elem, dag_elem)

    # Assign a Network dag to a given host
    def assignNetworkDAG(self, hostname, hosttype, dag):
        for element in self.networkDagElements(hostname, hosttype):
            if not self.writeCommand(element + ' ' + dag):
                return False
        return True

# If this library is run by itself, it does a unit test that
# connects to Click and configures its elements as an XIAEndHost
if __name__ == "__main__":
    hostname = socket.gethostname()
    hosttype = 'XIAEndHost'
    hid = 'HID:abf1014f0cc6b4d98b6748f23a7a8f22a3f7b199'
    network_dag = 'RE AD:abf1014f0cc6b4d98b6748f23a7a8f22a0000000'
    # Here's how this library should be used
    with ClickControl() as click:
        click.assignHID(hostname, hosttype, hid)
        click.assignNetworkDAG(hostnam, hosttype, dag)
