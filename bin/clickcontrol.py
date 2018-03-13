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

import os
import re
import sys
import socket
import xiapyutils
import nodeconf

# Principal types
principals = ['AD', 'HID', 'SID', 'CID', 'FID', 'IP']

# Pattern to read AD from a network DAG
adInDagPattern = re.compile('RE\s+(AD:\w+)')

# Click constants from etc/click/xia_constants.click
xiaconstantsclick = 'etc/click/xia_constants.click'
xiaconstantpattern = re.compile('define\((\$\w+)\s+([-\w]+)\)')

# Find the source directory 'xia-core' for this source tree
def get_srcdir():
    return xiapyutils.xia_srcdir()

def getxiaconstants():
    global xiaconstantsclick
    xia_constants = {}
    xiaconstantsclick = os.path.join(get_srcdir(), xiaconstantsclick)

    with open(xiaconstantsclick) as constants:
        for line in constants:
            match = xiaconstantpattern.match(line)
            if match:
                xia_constants[match.group(1)] = match.group(2)
    return xia_constants

class ClickControl:
    ''' Click controller
    Used to call write/read handlers inside click elements
    Specialized functions exist for assigning HID or Network DAG
    '''

    # Initialize a socket to talk to Click
    def __init__(self, clickhost='localhost', port=7777):
        self.initialized = False

        n = nodeconf.nodeconf()
        n.read()
        self.hostname = n.hostname()
        self.num_ifaces = int(n.numports())

        self.xia_constants = getxiaconstants()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.sock.connect((clickhost, port))
            self.initialized = True
            click_ver = self.sock.recv(1024)
            if "Click::ControlSocket" not in click_ver:
                print 'ERROR: Click did not provide version info on connect'
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

    # Call a read handler in Click
    def readCommand(self, command):
        cmd = 'read %s\n' % command
        self.send(cmd)
        response = self.receive()
        return response.split('\r\n')[-1]

    # Call a write handler in Click
    def writeCommand(self, command):
        cmd = 'write %s\n' % command
        self.send(cmd)
        response = self.receive()
        if 'OK' not in response:
            print 'ERROR: %s: Resulted in: %s\n' % (cmd, response)
            return False
        return True

    # Add a destined_for_localhost route table entry for given XID
    def addLocalRoute(self, hostname, xid):
        table = 'xrc/n/proc/rt_'
        if 'HID' in xid:
            table = table + 'HID'
        elif 'AD' in xid:
            table = table + 'AD'
        else:
            print 'ERROR: No table for xid', xid
            return False
        # Call write handler for
        destined_for_localhost = self.xia_constants['$DESTINED_FOR_LOCALHOST']
        cmd = '%s/%s.add %s %s' % (hostname, table, xid, destined_for_localhost)

        if not self.writeCommand(cmd):
            return False
        return True

    # Get a list of elements' write handlers
    def getElements(self, hostname, handler_name, iface_elem, elements):
		# FIXME: delete these lines?
        # Routing tables for each principal type
        #for principal in principals:
        #    elements.append('xrc/n/proc/rt_%s' % principal)

        # Iterate over the number of interfaces for this host type
        for i in range(self.num_ifaces):
            # Elements inside each interface card
            for elem in iface_elem:
                elements.append('xlc%d/%s' % (i, elem))

        # Prepend hostname to each hid_element
        return [hostname + '/' + e + '.' + handler_name for e in elements]

    # A list of all elements with HID write handler
    def hidElements(self, hostname):
        # Elements in line card that need to be notified
        iface_elem = ['x', 'xarpq', 'xarpr', 'xchal', 'xresp']

        # Xtransport and XCMP elements in RouteEngine and RoutingCore
        # FIXME: HACK - remove the FID entry once routing is correct
        hid_elem = ['xrc/xtransport', 'xrc/n/x', 'xrc/x', 'xrc/n/proc/x', 'xrc/n/proc/rt_FID']
        return self.getElements(hostname, 'hid', iface_elem, hid_elem)

    # Assign an HID to a given host
    def assignHID(self, hostname, hid):
        for element in self.hidElements(self.hostname):
            if not self.writeCommand(element + ' ' + hid):
                return False
        if not self.addLocalRoute(hostname, hid):
            return False
        return True

    # A list of all elements with Network DAG write handler
    def networkDagElements(self, hostname):
        # Elements in line card that need to be notified
        iface_elem = ['x', 'xchal']

        # Xtransport and XCMP elements in RouteEngine and RoutingCore
        dag_elem = ['xrc/xtransport', 'xrc/n/x', 'xrc/x', 'xrc/n/proc/x']
        return self.getElements(hostname, 'dag', iface_elem, dag_elem)

    # Assign a Network dag to a given host
    def assignDAG(self, hostname, dag):
        for element in self.networkDagElements(hostname):
            if not self.writeCommand(element + ' ' + dag):
                return False
        # Add a route to the local AD
        match = adInDagPattern.match(dag)
        if not match:
            print 'No AD found in Network DAG'
            return False
        ad = match.group(1)
        if not self.addLocalRoute(hostname, ad):
            return False
        return True

    def getParam(self, hostname, param):
        return self.readCommand(hostname + '/xrc/xtransport.' + param)

    def getDefaultAddr(self, hostname):
        return self.getParam(hostname, 'address')

    def getNSAddr(self, hostname):
        return self.getParam(hostname, 'nameserver')

    def getRVAddr(self, hostname):
        return self.getParam(hostname, 'rendezvous')

    def getRVControlAddr(self, hostname):
        return self.getParam(hostname, 'rendezvous_control')

    # Assign a Rendezvous DAG to an interface. All interfaces by default.
    def assignRVDAG(self, hostname, dag, iface=-1):
        cmd = '%s/xrc/xtransport.rvDAG %d,%s' % (hostname, iface, dag)
        if not self.writeCommand(cmd):
            return False
        return True

    # Assign a Rendezvous Control-plane DAG to an interface. default=all.
    def assignRVControlDAG(self, hostname, dag, iface=-1):
        cmd = '%s/xrc/xtransport.rvcDAG %d,%s' % (hostname, iface, dag)
        if not self.writeCommand(cmd):
            return False
        return True

    # Set an HID routing table entry
    def setHIDRoute(self, hostname, hid_str, port, flags):
        cmd = "{}/xrc/n/proc/rt_HID.set4 {},{},{},{}".format(
                hostname, hid_str, port, hid_str, flags)
        if not self.writeCommand(cmd):
            return False
        return True

    # Set nameserver DAG
    def setNSDAG(self, ns_dag):
        # TODO: Remove this hacky import.
        # Only python modules that have XIA swig in path can call this method
        import c_xsocket
        sockfd = c_xsocket.Xsocket(c_xsocket.SOCK_DGRAM)
        retval = c_xsocket.XupdateNameServerDAG(sockfd, ns_dag)
        c_xsocket.Xclose(sockfd)
        if retval != 0:
            print "Failed updating Nameserver DAG in Click"
            return False
        return True

    # TODO: Add setRVControlDAG function here.
    def setRVControlDAG(self, interface, control_plane_dag):
        print "setRVControlDAG called, but ignored"

    # Set XARP entry
    def setADInXARPTable(self, xid):
        if not xid.startswith('AD:'):
            print 'ClickControl.setADXARPEntry invalid AD {}'.format(xid)
            return False

        for i in range(self.num_ifaces):
            cmd = '{}/xlc{}/xarpr.ad {}'.format(self.hostname, i, xid)
            if not self.writeCommand(cmd):
                return False
        return True

# If this library is run by itself, it does a unit test that
# connects to Click and configures its elements as an XIAEndHost
if __name__ == "__main__":
    hostname = xiapyutils.getxiaclickhostname()
    hosttype = 'XIAEndHost'
    hid = 'HID:abf1014f0cc6b4d98b6748f23a7a8f22a3f7b199'
    dag = 'RE AD:abf1014f0cc6b4d98b6748f23a7a8f22a0000000' + ' ' + hid
    # Here's how this library should be used
    with ClickControl() as click:
        click.assignHID(hostname, hosttype, hid)
        click.assignDAG(hostname, hosttype, dag)
