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
import xiapyutils
from subprocess import Popen, PIPE

#add xia  port type on routeEntry
xiaconstanttype = 'routingeng/xia_constants.conf'
xiaconstantpattern = re.compile('define\((\$\w+)\s+([-\w]+)\)')

# Find the source directory 'xia-core' for this source tree
def get_srcdir():
    return xiapyutils.xia_srcdir()

def getxiaconstants():
    global xiaconstanttype
    xia_constants = {}
    xiaconstanttype = os.path.join(get_srcdir(), xiaconstanttype)
    # print 'Reading constant port type in %s' % xiaconstanttype
    with open(xiaconstanttype) as constants:
        for line in constants:
            match = xiaconstantpattern.match(line)
            if match:
                xia_constants[match.group(1)] = match.group(2)
   # print xia_constants
    return xia_constants

# Retrieve config file paths from arguments
if len(sys.argv) != 3:
    print 'Usage: %s <address.conf>' % sys.argv[0]
    sys.exit(-1)
nodesconf = sys.argv[1]
addressconf = sys.argv[2]
ns_sid = 'SID:1110000000000000000000000000000000001113'

# We create resolv.conf in the same directory as address.conf
resolvconfpath = os.path.join(os.path.dirname(addressconf), 'resolv.conf')

# Pattern to read AD from a network DAG
adInDagPattern = re.compile('RE\s+(AD:\w+)')

# Patterns to look for in config files
# e.g. router0 XIARouter4Port (AD:<cryptoAD> HID:<cryptoHID>)
# e.g. host0 XIAEndHost (HID:<cryptoHID>)
addrconfpattern = re.compile('^(\w+)\s+(\w+)\s+\((.+)\)')
nspattern = re.compile('^(\w+)\s+nameserver')

# e.g. "host0    XIAEndHost"
nodeconfpattern = re.compile('^(\w+)\s+(\w+)')

# Read in address.conf and configure ADs and HIDs in Click elements as needed
# NOTE: Also creates resolv.conf if RV and/or nameserver running on this router
def configure_click(config):
    resolvconf_lines = []
    ns_hosts = []
    destined_for_local = xia_constants['$DESTINED_FOR_LOCALHOST']

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
            print "match HostType from address.conf ", hosttype
            hid = match.group(3)

            # Routers have AD and HID in their arguments
            if 'Router' in hosttype:
                ad, hid = hid.split(' ')

            # Assign HID to this host
            #click.assignHID(hostname, hosttype, hid)

            # Write HID into forwarding table
            ps =Popen('./bin/xroute -a HID,%s,%s' % (hid, destined_for_local),shell=True , stdout=PIPE)
            ps_resp= ps.communicate()
            if ps.returncode !=0:
                print "Error: ", ps_resp[1]
            print "Added HID in forwardingtable: " , ps_resp[0].decode("utf-8")

            # Assign DAG to routers
            if ad:
                router_dag = 'RE %s %s' % (ad, hid)
                print "Check routerDAG ", router_dag
                # If nameserver running on this router, assign it a dag
                if hostname in ns_hosts:
                    resolvconf_lines.append('nameserver=%s %s' % (router_dag, ns_sid))

                # Finally, assign the DAG for this router in Click
                #click.assignDAG(hostname, hosttype, router_dag)

                #change to directly add into forwardingtable
                match_ad = adInDagPattern.match(router_dag)
                if not match_ad:
                    print 'No AD found in Network DAG'
                    return False
                ad_tmp = match_ad.group(1)
                print 'assignDAG for router', ad_tmp
                ps =Popen('./bin/xroute -a AD,%s,%s' % (ad_tmp, destined_for_local),shell=True , stdout=PIPE)
                ps_resp= ps.communicate()
                if ps.returncode !=0:
                    print "Error: ", ps_resp[1]
                print "Added routeAD in forwardingtable: " , ps_resp[0].decode("utf-8")

        # Write resolv.conf contents for nameserver
        if len(resolvconf_lines) > 0:
            # TODO: error out if a dag is not available for hostname
            with open(resolvconfpath, 'w') as resolvconf:
                for line in resolvconf_lines:
                    resolvconf.write('%s\n' % line)

if __name__ == "__main__":
    xia_constants = getxiaconstants()
    with open(addressconf, 'r') as config:
        configure_click(config)
