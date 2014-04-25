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

from __future__ import with_statement
import ConfigParser
import optparse
import glob
import os
from StringIO import StringIO

def update_value(filename, section, option, value):
    #python parser does not accept whitespace, remove it
    data = StringIO('\n'.join(line.strip() for line in open(filename)))
    config = ConfigParser.RawConfigParser()
    config.readfp(data)
    if config.has_section(section):
        if config.has_option(section, option):
            config.set(section, option, value)
            with open(filename, 'wb') as configfile:
                config.write(configfile)

if __name__ == "__main__":

    # no matter where your cwd is, go to our root dir
    full_path = os.path.realpath(__file__)
    os.chdir(os.path.dirname(full_path)+"/../..")

    parser = optparse.OptionParser("usage: %prog [options]")
    parser.add_option("-c", "--controller", dest="controller",
                          default=None, type="string",
                          help="specify controller file name, default: all ADs")
    parser.add_option("-s", "--service", dest="service",
                          default="", type="string",
                          help="specify the service name in the config file")
    parser.add_option("-o", "--option", dest="option",
                          default="", type="string",
                          help="specify the option name")
    parser.add_option("-v", "--value", dest="value",
                          default="", type="string",
                          help="specify the value of the option")
    (options, args) = parser.parse_args()

    if not options.controller: #all AD
        all_files = glob.glob("etc/controller*.ini")
        for cfgfile in all_files:
            update_value(cfgfile, options.service, 
                            options.option, options.value)
    else:
        update_value("etc/%s"%options.controller.lower(), 
                        options.service, options.option, options.value)

