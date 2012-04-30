#! /usr/bin/python
#
# script to generate click host and router config files

import sys
import uuid
import getopt
import socket
import subprocess
from string import Template

# constants
hostconfig = "host.click"
routerconfig = "router.click"
xia_addr = "xia_address.click"
ext = "template"

# default to host mode
nodetype = "host"
hostname = ""
adname = "AD_INIT"

#
# create a globally unique HID based off of our mac address
#
def createHID():
	hid = "HID:00000000"
	id = uuid.uuid1(uuid.getnode())
	return hid + id.hex

#
# make a short hostname
#
def getHostname():
	global hostname
	if (len(hostname) == 0):
		hostname = socket.gethostname()
	dot = hostname.find(".")
	if (dot >= 0):
		hostname = hostname[:dot]
	return hostname
	
#
# get a list of the interfaces and associated mac addresses on this machine
# 
# ignore eth0 as it is the control socket for the node, and also ignore any
# fake<n> interfaces as those are internal only
#
def getInterfaces():

	cmdline = subprocess.Popen(
			"ifconfig | grep HWaddr | grep -v fake | grep -v eth0 | tr -s ' ' | cut -d ' ' -f1,5",
#			"ifconfig | grep HWaddr | tr -s ' ' | cut -d ' ' -f1,5", 
			shell=True, stdout=subprocess.PIPE)
	result = cmdline.stdout.read().strip()

	interfaces = []
	addrs = result.split("\n")

	for addr in addrs:
		if (len(addr) != 0):
			(iface, mac) = addr.split()
			interfaces.append([iface, mac])

	return interfaces

#
# Fill in the xia_address template file
#
def makeXIAAddrConfig(hid):

	try:
		f = open(xia_addr + "." + ext, "r")
		text = f.read()
		f.close()

		f = open(xia_addr, "w")

	except:
		print "error opening file for reading and/or writing"
		sys.exit(-1)

	xchg = {}
	xchg['HNAME'] = getHostname()
	xchg['HID'] = hid

	s = Template(text)
	newtext = s.substitute(xchg)
	f.write(newtext)
	f.close()

#
# Fill in the host template file
#
def makeHostConfig(hid):

	interfaces = getInterfaces()

	if (len(interfaces) == 0):
		print "no available interface"
		exit(1)
	elif (len(interfaces) > 1):
		print "multiple interfaces found, using " + interfaces[0][0]

	try:
		f = open(hostconfig + "." + ext, "r")
		text = f.read()
		f.close()

		f = open(hostconfig, "w")

	except:
		print "error opening file for reading and/or writing"
		sys.exit(-1)

	xchg = {}
	xchg['HNAME'] = getHostname()
	xchg['ADNAME'] = adname
	xchg['HID'] = hid
	xchg['IFACE'] = interfaces[0][0]
	xchg['MAC'] = interfaces[0][1]

	# create $MAC0 thru $MAC3 replacements (not sure if needed for hosts??)
	i = 0
	while i < 4 and i < len(interfaces):
		repl = 'MAC' + str(i)
		xchg[repl] = interfaces[i][1]
		i += 1
	while i < 4:
		repl = 'MAC' + str(i)
		xchg[repl] = "00:00:00:00:00:00"
		i += 1

	s = Template(text)
	newtext = s.substitute(xchg)
	f.write(newtext)
	f.close()

#
# fill in the router template
#
# the router config file is broken into 4 sections
# header - router definition and include lines
# repeated block - with one section per interface
# extra section - discard mappping for unused ports on the 
#  router (depends on the number of interfaces)
# footer - boilerplate

def makeRouterConfig(hid):

	interfaces = getInterfaces()
	if (len(interfaces) < 2):
		print "error, not enough interfaces found for a router"
		exit(1)

	try:
		f = open(routerconfig + "." + ext, "r")
		text = f.read()
		f.close()

		f = open(routerconfig, "w")

	except:
		print "error opening file for reading and/or writing"
		sys.exit(-1)

	(header, body, extra, footer) = text.split("######")

	tpl = Template(header)

	xchg = {}
	xchg['ADNAME'] = adname
	xchg['HNAME'] = getHostname()
	xchg['HID'] = hid

	# create $MAC0 thru $MAC3 replacements
	i = 0
	while i < 4 and i < len(interfaces):
		repl = 'MAC' + str(i)
		xchg[repl] = interfaces[i][1]
		i += 1
	while i < 4:
		repl = 'MAC' + str(i)
		xchg[repl] = "00:00:00:00:00:00"
		i += 1
	 	
	newtext = tpl.substitute(xchg)

	i = 0
	tpl = Template(body)
	for (interface, mac) in interfaces:
		xchg['IFACE'] = interfaces[i][0]
		xchg['MAC'] = interfaces[i][1]
		xchg['NUM'] = i
#		xchg['NUMa'] = str(i) + "a"
#		xchg['NUMb'] = str(i) + "b"
		i += 1

		newtext += tpl.substitute(xchg)

		if i >= 4:
			break

	tpl = Template(extra)
	while i < 4:
		xchg['NUM'] = i
		newtext += tpl.substitute(xchg)
		i += 1

	tpl = Template(footer)
	newtext += tpl.substitute(xchg)

	f.write(newtext)
	f.close()

#
# parse the command line so we can do stuff
#
def getOptions():
	global hostname
	global nodetype
	global adname
	try:
		shortopt = "hrn:a:"
		opts, args = getopt.getopt(sys.argv[1:], shortopt, 
			["help", "router", "name=", "ad="])
	except getopt.GetoptError, err:
		# print	 help information and exit:
		print str(err) # will print something like "option -a not recognized"
		help()
		sys.exit(2)

	for o, a in opts:
		if o in ("-h", "--help"):
			help()
		elif o in ("-a", "--ad"):
			adname = a
		elif o in ("-n", "--name"):
			hostname = a
		elif o in ("-r", "--router"):
			nodetype = "router"
		else:
		 	assert False, "unhandled option"

#
# display helpful information
#
def help():
	print """
usage: xconfig [-h] [-r] [-n hostname]
where:
  -h            : get help
  --help

  -n <name>     : set HID name tp <name>
  --name=<name>

  -a <name>     : set AD name to <name>
  --ad=<name>

  -r            : do router config instead of host
  --router
"""
	sys.exit()

#
# do it
#
def main():

	getOptions()

	hid = createHID()
	makeXIAAddrConfig(hid)

	if (nodetype == "host"):
		makeHostConfig(hid)
	else:
		makeRouterConfig(hid)

if __name__ == "__main__":
    main()
