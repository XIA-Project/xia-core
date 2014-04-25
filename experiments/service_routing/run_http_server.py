#!/usr/bin/env python
#ts=4
#
# Copyright 2014 Carnegie Mellon University
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
#

import signal
import sys
import os
import subprocess
import socket
import re
import ConfigParser
import optparse
from StringIO import StringIO

http = None
http_xia = None
options = None

Iamcontroller = False
Iaminstance = False
hostname = socket.gethostname()
controller_file = ""

def signal_handler(s, frame):
    if Iaminstance:
        print 'Killing http service..'
        #kill them, NOTE: python 2.5 does not have Popen.kill()
        if http:
            os.kill(http.pid, s)
        if http_xia:
            os.kill(http_xia.pid, s)

    #disable it
    if Iamcontroller:
        print 'Disabling service ID'
        subprocess.Popen(
            'experiments/service_routing/set_service.py -c %s ' \
             '-s httpSID -o enabled -v 0'% controller_file,
             shell=True)

    sys.exit(0)

if __name__ == "__main__":

    parser = optparse.OptionParser("usage: %prog [options]")
    parser.add_option("-s", "--section", dest="section_name",
                          default=-1, type="string",
                          help="specify AD number, default: all ADs")
    (options, args) = parser.parse_args()

    full_path = os.path.realpath(__file__)
    os.chdir(os.path.dirname(full_path))

    data = StringIO('\n'.join(line.strip() for line in open('deploy.ini')))
    config = ConfigParser.RawConfigParser()
    config.readfp(data)
    if config.has_section(options.section_name):
        if config.has_option(options.section_name, 'controller'):
            controller_name = config.get(options.section_name, 'controller')
            if re.search(controller_name, hostname, re.I):
                Iamcontroller = True
        else:
            print "Missing option 'controller'"
            sys.exit(-1)
        if config.has_option(options.section_name, 'instance'):
            instance_name = config.get(options.section_name, 'instance')
            if re.search(instance_name, hostname, re.I):
                Iaminstance = True
        else:
            print "Missing option 'instance'"
            sys.exit(-1)
        if config.has_option(options.section_name, 'controller_file'):
            controller_file = config.get(options.section_name, 
                           'controller_file')
        else:
            print "Missing option 'controller_file'"
            sys.exit(-1)
    else:
        print "Missing section %s" % options.section_name
        sys.exit(-1)


    # no matter where your cwd is, go to our root dir
    os.chdir(os.path.dirname(full_path)+"/../..")

    signal.signal(signal.SIGINT, signal_handler)

    if Iamcontroller:
        print 'Enabling service ID...'
        #enable it
        subprocess.Popen(
            'experiments/service_routing/set_service.py -c %s -s httpSID ' \
            '-o enabled -v 1' % controller_file,
             shell=True)
    if Iaminstance:
        print 'starting http service...'
        #start http
        http = subprocess.Popen('./httpd', 
                                cwd='bin/xwrap applications/tinyhttpd', 
                                shell=True)
        #start http -xia
        http_xia = subprocess.Popen('bin/xwrap applications/tinyhttpd/httpd -x', 
                                    shell=True)
        print 'Press Ctrl+C to stop'
    signal.pause()
