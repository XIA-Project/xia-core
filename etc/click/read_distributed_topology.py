#!/usr/bin/env python
# prepare config file for a true distributed XIA network
# translate the overall topology into real host names and sock connections among them.
# filter only the part related to this (local) host to start for bin/xianet
# Currently only works for GENI as topology part is not down yet
from __future__ import with_statement
import socket
import sys
import os
import subprocess

#sys.argv[1]: path to mapping file
#sys.argv[2]: path to the overall topology file (e.g. path to xia_topology.click)


host_mapping = {}
path_to_mapping_file = os.path.abspath("etc/click/host_mapping.conf")
path_to_topology_file = os.path.abspath("etc/click/xia_topology.click")
path_to_local_click_file = os.path.abspath("etc/click/xia_topology_local.click")

if len(sys.argv) >= 2:
    path_to_mapping_file = sys.argv[1]

if len(sys.argv) >= 3:
    path_to_topology_file = sys.argv[2]

my_hostname = socket.gethostname()
my_xianame = ""
my_AD = ""
my_HID = ""
type_flag = "" # c: controller, t: host, r: router

with open(path_to_mapping_file) as map_file:
    for line in map_file:
        (xia_hostname, realworld_hostname, AD, HID, flag) = line.rstrip().split(',')
        host_mapping[xia_hostname] = realworld_hostname
        #find who I am
        if realworld_hostname == my_hostname:
            my_xianame = xia_hostname
            my_AD = AD
            my_HID = HID
            type_flag = flag

#works on GENI: call xconfig leave the rest to click
cmd = "python xconfig.py -i %s -a %s -H %s -%s -f eth999 -o ../xia_topology_local.click"%(my_xianame, my_AD, my_HID, type_flag)
print cmd
subprocess.Popen(cmd, cwd='etc/click/templates', shell=True)

"""
config_type = "" #e.g.  host0 :: XIAEndHost (RE AD0 HID0, HID0, 1500, 0, aa:aa:aa:aa:aa:aa);
config_links = [] #e.g. controller0[0] -> LinkUnqueue(0.005, 1 GB/s) -> [0]router0; or router0[3] -> Idle;

with open(path_to_topology_file) as topology_file:
    for line in topology_file:
        if line.find(my_xianame) == 0: #this line starts with my xia name
            if line.find('LinkUnqueue') > 0 or line.find('Idle') > 0:
                config_links.append(line)
            else:
                if config_type != "":
                    print "error multiple config_type line found", config_type, "and", line
                config_type = line

print "my config_type is", config_type
#print "my config_links are", config_links

with open(path_to_local_click_file, 'w') as config_file:
    config_file.write("require(library xia_router_lib.click);\n")
    config_file.write("require(library xia_address.click);\n")
    config_file.write(config_type)
    config_file.write('\n')
    config_file.write('FromDevice(eth0, METHOD LINUX) -> [0]' + my_xianame + '[0] -> ToDevice(eth0)\n')
    config_file.write('ControlSocket(tcp, 7777);')
"""








