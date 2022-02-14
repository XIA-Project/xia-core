#!/usr/bin/env python2.7
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


# Retrieve config file paths from arguments
if len(sys.argv) != 3:
    print 'Usage: %s <address.conf>' % sys.argv[0]
    sys.exit(-1)
nodesconf = sys.argv[1]
addressconf = sys.argv[2]
ns_sid = 'SID:1110000000000000000000000000000000001113'

# We create resolv.conf in the same directory as address.conf
resolvconfpath = os.path.join(os.path.dirname(addressconf), 'resolv.conf')

# Patterns to look for in config files

# e.g. router0 XIARouter4Port (AD:<cryptoAD> HID:<cryptoHID>)
# e.g. host0 XIAEndHost (HID:<cryptoHID>)
addrconfpattern = re.compile('^(\w+)\s+(\w+)\s+\((.+)\)')
nspattern = re.compile('^(\w+)\s+nameserver')

# e.g. "host0    XIAEndHost"
nodeconfpattern = re.compile('^(\w+)\s+(\w+)')

# Read in address.conf and configure ADs and HIDs in Click elements as needed
# NOTE: Also creates resolv.conf if RV and/or nameserver running on this router
def configure_click(click, config):
    resolvconf_lines = []
    ns_hosts = []

    for line in config:
        match = nspattern.match(line)
        if match:
            hostname = match.group(1)
            ns_hosts.append(hostname)
    
    #look for host, router and nameserver entries
    config.seek(0)
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

            # Assign DAG to routers
            if ad:
                router_dag = 'RE %s %s' % (ad, hid)
                # If nameserver running on this router, assign it a dag
                if hostname in ns_hosts:
                    resolvconf_lines.append('nameserver=%s %s' % (router_dag, ns_sid))

                # Finally, assign the DAG for this router in Click
                click.assignDAG(hostname, hosttype, router_dag)

        # Write resolv.conf contents for nameserver
        if len(resolvconf_lines) > 0:
            # TODO: error out if a dag is not available for hostname
            with open(resolvconfpath, 'w') as resolvconf:
                for line in resolvconf_lines:
                    resolvconf.write('%s\n' % line)

if __name__ == "__main__":
    with open(addressconf, 'r') as config:
        with clickcontrol.ClickControl() as click:
            configure_click(click, config)

