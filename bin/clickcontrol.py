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

# Number of interfaces for each host type
numIfaces = {'XIAEndHost':4, 'XIARouter4Port':4, 'XIARouter8Port':8, 'XIARouter2Port':2}

# Principal types
principals = ['AD', 'HID', 'SID', 'CID', 'FID', 'IP']

# Pattern to read AD from a network DAG
adInDagPattern = re.compile('RE\s+(AD:\w+)')

# Click constants from etc/click/xia_constants.click
xiaconstanttype = 'routingeng/xia_constants.conf'
xiaconstantpattern = re.compile('define\((\$\w+)\s+([-\w]+)\)')

# Find the source directory 'xia-core' for this source tree
def get_srcdir():
    return xiapyutils.xia_srcdir()

def getxiaconstants():
    global xiaconstanttype
    xia_constants = {}
    xiaconstanttype = os.path.join(get_srcdir(), xiaconstanttype)
    with open(xiaconstanttype) as constants:
        for line in constants:
            match = xiaconstantpattern.match(line)
            if match:
                xia_constants[match.group(1)] = match.group(2)
    print xia_constants
    return xia_constants

class ClickControl:
    ''' Click controller
    Used to call write/read handlers inside click elements
    Specialized functions exist for assigning HID or Network DAG
    '''

    # Initialize a socket to talk to Click
    def __init__(self, clickhost='localhost', port=7777):
        self.initialized = False
        self.xia_constants = getxiaconstants()
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.sock.connect((clickhost, port))
            self.initialized = True
            click_ver = self.sock.recv(1024)
            if "Click::ControlSocket" not in click_ver:
                print 'ERROR: Click did not provide version info on connect'
            else:
                print 'Connected to {}'.format(click_ver)
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
        print 'Check Adding localroute: %s' % cmd
        if not self.writeCommand(cmd):
            return False
        return True

    # Assign an HID to a given host
    def assignHID(self, hostname, hosttype, hid):
        if not self.addLocalRoute(hostname, hid):
            return False
        print 'Check: add to forwarding table'
        testport = self.xia_constants['$DESTINED_FOR_LOCALHOST']
        testroute = "./bin/xroute -a HID,{},{}".format(
                    hid, testport)
        print testroute
        return True

    # Assign a Network dag to a given host
    def assignDAG(self, hostname, hosttype, dag):
        match = adInDagPattern.match(dag)
        if not match:
            print 'No AD found in Network DAG'
            return False
        ad = match.group(1)
        print 'assignDAG', match
        if not self.addLocalRoute(hostname, ad):
            return False
        return True

    def assignXcacheAID(self, hostname, aid):
        cmd = '{}/xrc/n/proc/rt_ICID.xcache {}'.format(hostname, aid)
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
