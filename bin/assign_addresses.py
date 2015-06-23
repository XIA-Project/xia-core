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

import re
import sys
import os.path
import genkeys
import clickcontrol

# NOTE: We only support one HID for all interfaces in multihoming Phase 1

# Retrieve config file paths from arguments
if len(sys.argv) != 3:
    print 'Usage: %s <nodes.conf> <address.conf>' % sys.argv[0]
    sys.exit(-1)

nodesconf = sys.argv[1]
addressconf = sys.argv[2]
ns_sid = 'SID:1110000000000000000000000000000000001113'

# We create resolv.conf in the same directory as address.conf
resolvconfpath = os.path.join(os.path.dirname(addressconf), 'resolv.conf')
router_dags = {}

# Patterns to look for in config files

# e.g. router0 XIARouter4Port (AD:<cryptoAD> HID:<cryptoHID>)
# e.g. host0 XIAEndHost (HID:<cryptoHID>)
addrconfpattern = re.compile('^(\w+)\s+(\w+)\s+\((.+)\)')
nspattern = re.compile('^(\w+)\s+nameserver')

# e.g. "host0    XIAEndHost"
nodeconfpattern = re.compile('^(\w+)\s+(\w+)')

# Assign ADs and HIDs for routers and just HIDs for hosts
def assign_xids(outfile, hostname, hosttype):
    # Nameserver entry has no args
    if hosttype == 'nameserver':
        outfile.write('%s %s\n' % (hostname, hosttype))
        return

    # All other host/router entries have arguments in parentheses
    outfile.write('%s %s (' % (hostname, hosttype))

    # Assign an AD for every router
    if 'Router' in hosttype:
        outfile.write('%s ' % genkeys.create_new_AD())

    # Assign the same HID for every interface this host has
    hid = genkeys.create_new_HID()
    outfile.write(hid)
    #for i in range(clickcontrol.numIfaces[hosttype]):
        #outfile.write(' %s' % hid)

    # Completed writing the address.conf
    outfile.write(')\n')

# Read in nodes.conf and write address.conf with new ADs and HIDs
def process_config(infile, outfile):
    for line in infile:
        match = nodeconfpattern.match(line)
        if match:
            assign_xids(outfile, match.group(1), match.group(2))

# Read in address.conf and configure ADs and HIDs in Click elements as needed
def configure_click(click, config):
    for line in config:
        ad = None
        # Read in XIA host addresses from address.conf
        match = addrconfpattern.match(line)
        if match:
            hostname = match.group(1)
            hosttype = match.group(2)
            hid = match.group(3)
            # Routers have AD and HID in their arguments
            if 'Router' in hosttype:
                ad, hid = hid.split(' ')
            # Assign HID to this host
            click.assignHID(hostname, hosttype, hid)
            # Assign AD to routers
            if ad:
                network_dag = 'RE %s' % ad
                click.assignNetworkDAG(hostname, hosttype, network_dag)
                router_dags[hostname] = network_dag + ' ' + hid
        # Write resolv.conf if nameserver runs here
        match = nspattern.match(line)
        if match:
            # TODO: error out if a dag is not available for hostname
            with open(resolvconfpath, 'w') as resolvconf:
                resolvconf.write('nameserver=%s %s\n' % (router_dags[hostname], ns_sid))

if __name__ == "__main__":
    # If address.conf doesn't exist create it
    # TODO: Also ensure timestamp of nodes.conf is not newer than address.conf
    if not os.path.isfile(addressconf):
        print 'Creating %s' % addressconf
        with open(nodesconf, 'r') as infile:
            with open(addressconf, 'w') as outfile:
                process_config(infile, outfile)
    else:
        print 'Using existing addresses from %s' % addressconf

    # Now address.conf exists, so open and process it
    with open(addressconf, 'r') as config:
        with clickcontrol.ClickControl() as click:
            configure_click(click, config)
