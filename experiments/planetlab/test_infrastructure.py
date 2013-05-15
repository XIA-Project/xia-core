#!/usr/bin/python 

import commands
from subprocess import call

my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]
my_commands = []

try: 
    f = open('tunneling.ini', 'r')
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
    call(command,shell=True)


