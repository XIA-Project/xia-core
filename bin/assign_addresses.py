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

# NOTE: We only support one HID for all interfaces in multihoming Phase 1


ns_sid = 'SID:1110000000000000000000000000000000001113'


verbose = True

# Patterns to look for in config files

# e.g. router0 XIARouter4Port (AD:<cryptoAD> HID:<cryptoHID>)
# e.g. host0 XIAEndHost (HID:<cryptoHID>)
addrconfpattern = re.compile('^(\w+)\s+(\w+)\s+\((.+)\)')
nspattern = re.compile('^(\w+)\s+nameserver')
rvpattern = re.compile('^(\w+)\s+rendezvous\s+(.+)\s+(.+)')

# e.g. "host0    XIAEndHost"
nodeconfpattern = re.compile('^(\w+)\s+(\w+)')


def message(msg):
    if verbose:
        print msg

# Assign XIDs as needed for RV, Routers etc.
# NS:     <hostname> nameserver
# RV:     <hostname> rendezvous (<rv_SID> <rvc_SID>)
# Router: <hostname> <hosttype> (AD:<new_AD> HID:<new_HID>)
# Host:   <hostname> <hosttype> (HID:<new_HID>)
def assign_xids(outfile, hostname, hosttype):
    # Nameserver entry has no args
    if hosttype == 'nameserver':
        outfile.write('%s %s\n' % (hostname, hosttype))
        return

    # Rendezvous entry has no args
    if hosttype == 'rendezvous':
        rv_sid = genkeys.create_new_SID()
        rvc_sid = genkeys.create_new_SID()
        outfile.write('%s %s %s %s\n' % (hostname, hosttype, rv_sid, rvc_sid))
        return

    # All other host/router entries have arguments in parentheses
    outfile.write('%s %s (' % (hostname, hosttype))

    # Assign an AD for routers running a controller
    if ('Controller' in hosttype) or ('Router' in hosttype and iscontroller):
        outfile.write('%s ' % genkeys.create_new_AD())

    # Assign the same HID for every interface this host has
    hid = genkeys.create_new_HID()
    outfile.write(hid)

    # Completed writing the address.conf
    outfile.write(')\n')

# Read in nodes.conf and write address.conf with new ADs and HIDs
def create_addrconf(infile, outfile):
    for line in infile:
        match = nodeconfpattern.match(line)
        if match:
            assign_xids(outfile, match.group(1), match.group(2))

# Read in address.conf and configure ADs and HIDs in Click elements as needed
# NOTE: Also creates resolv.conf if RV and/or nameserver running on this router
def configure_click(click, config):
    rendezvous_hosts = {}
    resolvconf_lines = []
    ns_hosts = []

    # First pass - look for rendezvous entries
    for line in config:
        match = rvpattern.match(line)
        if match:
            hostname = match.group(1)
            rv_sid = match.group(2)
            rvc_sid = match.group(3)
            rendezvous_hosts[hostname] = (rv_sid, rvc_sid)
        match = nspattern.match(line)
        if match:
            hostname = match.group(1)
            ns_hosts.append(hostname)

    # Second pass - look for host, router and nameserver entries
    # NOTE: Assumes nameserver line is after all router lines in address.conf
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
            if ('Controller' in hosttype) or ('Router' in hosttype and iscontroller):
                ad, hid = hid.split(' ')

            # Assign HID to this host
            click.assignHID(hostname, hosttype, hid)

            # Assign DAG to routers
            if ad:
                router_dag = 'RE %s %s' % (ad, hid)
                router_rv_dag = ''

                # If rendezvous chosen, assign a fallback also
                if hostname in rendezvous_hosts:
                    rv_sid, rvc_sid = rendezvous_hosts[hostname]
                    rv_fallback = '%s %s %s' % (ad, hid, rv_sid)
                    rvc_dag = 'RE %s %s %s' % (ad, hid, rvc_sid)
                    router_rv_dag = 'RE %s ( %s ) %s' % (ad, rv_fallback, hid)
                    resolvconf_lines.append('rendezvous=%s' % (router_rv_dag))
                    resolvconf_lines.append('rendezvousc=%s' % (rvc_dag))

                # If nameserver running on this router, assign it a dag
                # NOTE: Even the nameserver gets RV DAG, if available
                if hostname in ns_hosts:
                    resolvconf_lines.append('nameserver=%s %s' % (router_dag, ns_sid))

                # Finally, assign the DAG for this router in Click
                click.assignDAG(hostname, hosttype, router_dag)
                if(len(router_rv_dag) > len(router_dag)):
                    click.assignRVDAG(hostname, hosttype, router_rv_dag)
                    click.assignRVControlDAG(hostname, hosttype, rvc_dag)

                # Add AD to the XARP tables on all line cards
                if click.setADInXARPTable(ad) == False:
                    print "Error adding AD to XARP tables"
                    sys.exit(-1)

        # Write resolv.conf contents
        if len(resolvconf_lines) > 0:
            # TODO: error out if a dag is not available for hostname
            with open(resolvconfpath, 'w') as resolvconf:
                for line in resolvconf_lines:
                    resolvconf.write('%s\n' % line)

def main():
    # Retrieve config file paths from arguments
    if len(sys.argv) != 4:
        print 'Usage: %s  <nodes.conf> <address.conf> <router_type>' % sys.argv[0]
        sys.exit(-1)

    nodesconf = sys.argv[1]
    addressconf = sys.argv[2]
    router_type = sys.argv[3]

    iscontroller = False
    if router_type == "--controller":
        iscontroller = True

    # We create resolv.conf in the same directory as address.conf
    resolvconfpath = os.path.join(os.path.dirname(addressconf), 'resolv.conf')

    # If address.conf doesn't exist create it
    # TODO: Also ensure timestamp of nodes.conf is not newer than address.conf
    if not os.path.isfile(addressconf):
        message('Creating %s' % addressconf)
        with open(nodesconf, 'r') as infile:
            with open(addressconf, 'w') as outfile:
                create_addrconf(infile, outfile)
    else:
        message('Using existing addresses from %s' % addressconf)

    # Now address.conf exists, so open and process it
    with open(addressconf, 'r') as config:
        with clickcontrol.ClickControl() as click:
            configure_click(click, config)

if __name__ == "__main__":
    main()

