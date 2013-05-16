#!/usr/bin/python 

import commands, sys
from subprocess import call

my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]
my_commands = []

if len(sys.argv) < 2:
    print "Usage: %s [path/to/cmd_file]" % sys.argv[0]
    sys.exit(-1)

cmd_file = sys.argv[1]

try: 
    f = open(cmd_file, 'r')
    sections = f.read().split('[')
    for section in sections:
        ip = section.split(']')[0]
        if ip == my_ip:
            my_commands = section.split('\n')[1:-1]
    f.close()
except Exception, e: 
    print e

# some quick testing
for command in my_commands:
    print command
    call(command,shell=True)


