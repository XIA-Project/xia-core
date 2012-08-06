#! /usr/bin/python #
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
dualhostconfig = "dual_stack_host.click"
dualrouterconfig = "dual_stack_router.click"
xia_addr = "xia_address.click"
ext = "template"

# default to host mode
nodetype = "host"
dual_stack = False
hostname = ""
adname = "AD_INIT"
nameserver = "no"
nameserver_ad = "AD_NAMESERVER"
nameserver_hid = "HID_NAMESERVER"
ip_override_addr = None
interface_filter = None
interface = None

#
# create a globally unique HID based off of our mac address
#
def createHID(prefix="00000000"):
	hid = "HID:" + prefix
	id = uuid.uuid1(uuid.getnode())
	return hid + id.hex
	
#
# create a globally unique AD based off of router's mac address
#
def createAD():
	ad = "AD:10000000"
	id = uuid.uuid1(uuid.getnode())
	return ad + id.hex	

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
	return ''.join(char for char in hostname if char.isalnum())  # make sure we don't return characters illegal in click element names
	
#
# get a list of the interfaces and associated mac addresses on this machine
# 
# ignore any interfaces matching 'filter' (e.g., eth0 on GENI or wlan0 on the
# demo Giadas as it is the control socket for the node) and also ignore any
# fake<n> interfaces as those are internal only
#
def getInterfaces(ignore_interfaces, specific_interface):
	
	# Get the default gateway  TODO: should there be one per interface?
	cmdline = subprocess.Popen(
			"route -n | grep ^0.0.0.0 | tr -s ' ' | cut -d ' ' -f2",
			shell=True, stdout=subprocess.PIPE)
	default_gw = cmdline.stdout.read().strip()


	# Get the MAC and IP addresses
	filters = ''
	if ignore_interfaces != None:
		filter_array = ignore_interfaces.split(",")
		for filter in filter_array:
			filters += 'grep -v %s | ' % filter.strip()
	use_interface = ''
	if specific_interface != None:
		use_interface = 'grep -A 1 %s | ' % specific_interface
	
	cmdline = subprocess.Popen(
			"ifconfig | %s grep -v fake | grep -A 1 HWaddr | %s sed 's/ \+/ /g' | sed s/addr://" % (filters, use_interface),  
			shell=True, stdout=subprocess.PIPE)
	result = cmdline.stdout.read().strip()

	# TODO: eliminate this by making the shell command above smarter
	addrs = []
	temp_array = result.split("\n")
	iface, mac, ip = (None, None, None)
	for i in range(len(temp_array)):
		if temp_array[i].strip() == '':
			continue
		if i % 3 == 0:
			iface = temp_array[i].split(' ')[0]
			mac = temp_array[i].split(' ')[4]
		elif i % 3 == 1:
			ip = temp_array[i].split(' ')[2]
			addrs.append([iface, mac, ip])

	#interfaces = []
	#addrs = result.split("\n")
	#print addrs

	interfaces = []
	#for addr in addrs:
	#	if (len(addr) != 0):
	#		(iface, mac) = addr.split()
	#		interfaces.append([iface, mac])
	for addr in addrs:
		if (len(addr) != 0):
			(iface, mac, ip) = addr#.split()
			if ip_override_addr != None:
				external_ip = ip_override_addr
			else:
				external_ip = ip
			interfaces.append([iface, mac, ip, default_gw, external_ip])

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

	interfaces = getInterfaces(interface_filter, interface)

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
		
	if (nameserver == "no"):
		xchg['HID'] = hid
	else:
		xchg['HID'] = nameserver_hid	
	
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

def makeRouterConfig(ad, hid):
	global interface_filter, interface

	interfaces = getInterfaces(interface_filter, interface)
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
	if (nameserver == "no"):
		xchg['ADNAME'] = ad
	else:
		xchg['ADNAME'] = nameserver_ad		
	xchg['HNAME'] = getHostname()
	xchg['HID'] = hid

	if ip_override_addr == None:
		xchg['EXTERNAL_IP'] = '0.0.0.0'
	else:
		xchg['EXTERNAL_IP'] = ip_override_addr

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
	for interface in interfaces:
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

##
## fill in the dual-stack host config
##
## the router config file is broken into 4 sections
## header - router definition and include lines
## used ports - connects used ports to interfaces (currently we just use port 0)
## extra section - discard mappping for unused ports on the 
##  router (currently ports 1-3)
## footer - boilerplate
#
#def makeDualHostConfig(ad, hid, rhid):
#	global interface_filter, interface
#
#	interfaces = getInterfaces(interface_filter, interface) 
#	print 'Making dual stack host config'
#
#	try:
#		f = open(dualhostconfig + "." + ext, "r")
#		text = f.read()
#		f.close()
#
#		f = open(dualhostconfig, "w")
#
#	except:
#		print "error opening file for reading and/or writing"
#		sys.exit(-1)
#
#	(header, body, extra, footer) = text.split("######")
#
#	tpl = Template(header)
#
#	xchg = {}
#	if (nameserver == "no"):
#		xchg['ADNAME'] = ad
#		xchg['HID'] = hid
#	else:
#		xchg['ADNAME'] = nameserver_ad		
#		xchg['HID'] = nameserver_hid	
#	xchg['RHID'] = rhid
#	xchg['HNAME'] = getHostname()
#	
#	if ip_override_addr == None:
#		xchg['EXTERNAL_IP'] = '0.0.0.0'
#	else:
#		xchg['EXTERNAL_IP'] = ip_override_addr
#
#	# create $MAC0 thru $MAC3 replacements
#	i = 0  
#	while i < 3 and i < len(interfaces):  # Only go to 2 because the host is connected to router port 3
#		repl = 'MAC' + str(i)
#		xchg[repl] = interfaces[i][1]
#		repl = 'IPADDR' + str(i)
#		xchg[repl] = interfaces[i][2]
#		repl = 'GWADDR' + str(i)
#		xchg[repl] = interfaces[i][3]
#		repl = 'EXT_IPADDR' + str(i)
#		xchg[repl] = interfaces[i][4]
#		i += 1
#	while i < 3:
#		repl = 'MAC' + str(i)
#		xchg[repl] = "00:00:00:00:00:00"
#		repl = 'IPADDR' + str(i)
#		xchg[repl] = "1.1.1.1"
#		repl = 'GWADDR' + str(i)
#		xchg[repl] = "1.1.1.1"
#		repl = 'EXT_IPADDR' + str(i)
#		xchg[repl] = "1.1.1.1"
#		i += 1
#	 	
#	newtext = tpl.substitute(xchg)
#
#	i = 0
#	tpl = Template(body)
#	for (interface, mac, ip, gw, ext_ip) in interfaces:
#		xchg['IFACE'] = interfaces[i][0]
#		xchg['MAC'] = interfaces[i][1]
#		xchg['NUM'] = i
##		xchg['NUMa'] = str(i) + "a"
##		xchg['NUMb'] = str(i) + "b"
#		i += 1
#
#		newtext += tpl.substitute(xchg)
#
#		if i >= 3:
#			break
#
#	tpl = Template(extra)
#	while i < 3:
#		xchg['NUM'] = i
#		newtext += tpl.substitute(xchg)
#		i += 1
#
#	tpl = Template(footer)
#	newtext += tpl.substitute(xchg)
#
#	f.write(newtext)
#	f.close()


#
# fill in the dual-stack host config
#
# The dual-stack hosts consists of a host element
# connected to a dual-stack router. On the router,
# for now we assume that port 3 is connected to IP,
# port 2 is connected to the host, and the
# remaining ports are idle or connected to XIA networks
#
# the router config file is broken into 4 sections
# header - router definition and include lines
# used ports - connects used ports to interfaces 
# extra section - discard mappping for unused ports on the router 
# footer - boilerplate

def makeDualHostConfig(ad, hid, rhid):
	global interface_filter, interface

	if interface_filter == None:
		filter = interface
	else:
		filter = '%s,%s' % (interface, interface_filter)

	ip_interface = getInterfaces(None, interface)
	xia_interfaces = getInterfaces(filter, None) 

	try:
		f = open(dualhostconfig + "." + ext, "r")
		text = f.read()
		f.close()

		f = open(dualhostconfig, "w")

	except:
		print "error opening file for reading and/or writing"
		sys.exit(-1)

	(header, body, extra, footer) = text.split("######")

	tpl = Template(header)

	xchg = {}
	if (nameserver == "no"):
		xchg['ADNAME'] = ad
		xchg['HID'] = hid
	else:
		xchg['ADNAME'] = nameserver_ad		
		xchg['HID'] = nameserver_hid
	xchg['RHID'] = rhid
	xchg['HNAME'] = getHostname()
	
	xchg['EXTERNAL_IP'] = ip_interface[0][4]

	# Handle the pure XIA ports (0-1)
	i = 0  
	while i < 2 and i < len(xia_interfaces):  # Only go to 1 because port 2 is connected to the host and 3 is connected to IP
		repl = 'IP_ACTIVE' + str(i)
		xchg[repl] = str(0)
		repl = 'MAC' + str(i)
		xchg[repl] = xia_interfaces[i][1]
		repl = 'IPADDR' + str(i)
		xchg[repl] = xia_interfaces[i][2]
		repl = 'GWADDR' + str(i)
		xchg[repl] = xia_interfaces[i][3]
		repl = 'EXT_IPADDR' + str(i)
		xchg[repl] = xia_interfaces[i][4]
		i += 1
	while i < 2:
		repl = 'IP_ACTIVE' + str(i)
		xchg[repl] = str(0)
		repl = 'MAC' + str(i)
		xchg[repl] = "00:00:00:00:00:00"
		repl = 'IPADDR' + str(i)
		xchg[repl] = "1.1.1.1"
		repl = 'GWADDR' + str(i)
		xchg[repl] = "1.1.1.1"
		repl = 'EXT_IPADDR' + str(i)
		xchg[repl] = "1.1.1.1"
		i += 1

	# Handle the IP port (3)
	xchg['IP_ACTIVE3'] = str(1)
	xchg['MAC3'] = ip_interface[0][1];
	xchg['IPADDR3'] = ip_interface[0][2]
	xchg['GWADDR3'] = ip_interface[0][3]
	xchg['EXT_IPADDR3'] = ip_interface[0][4]
	 	
	newtext = tpl.substitute(xchg)

	i = 0
	tpl = Template(body)
	for (interface, mac, ip, gw, ext_ip) in xia_interfaces:
		xchg['IFACE'] = xia_interfaces[i][0]
		xchg['MAC'] = xia_interfaces[i][1]
		xchg['NUM'] = i
		i += 1

		newtext += tpl.substitute(xchg)

		if i >= 2:
			break

	xchg['IFACE'] = ip_interface[0][0]
	xchg['MAC'] = ip_interface[0][1]
	xchg['NUM'] = 3
	newtext += tpl.substitute(xchg)

	tpl = Template(extra)
	while i < 2:
		xchg['NUM'] = i
		newtext += tpl.substitute(xchg)
		i += 1

	tpl = Template(footer)
	newtext += tpl.substitute(xchg)

	f.write(newtext)
	f.close()


#
# fill in the dual-stack router config
#
# For now we assume that port 3 is connected to IP and the
# remaining ports are connected to XIA networks
#
# the router config file is broken into 4 sections
# header - router definition and include lines
# used ports - connects used ports to interfaces 
# extra section - discard mappping for unused ports on the router 
# footer - boilerplate

def makeDualRouterConfig(ad, rhid):
	global interface_filter, interface

	if interface_filter == None:
		filter = interface
	else:
		filter = '%s,%s' % (interface, interface_filter)

	ip_interface = getInterfaces(None, interface)
	xia_interfaces = getInterfaces(filter, None) 

	try:
		f = open(dualrouterconfig + "." + ext, "r")
		text = f.read()
		f.close()

		f = open(dualrouterconfig, "w")

	except:
		print "error opening file for reading and/or writing"
		sys.exit(-1)

	(header, body, extra, footer) = text.split("######")

	tpl = Template(header)

	xchg = {}
	if (nameserver == "no"):
		xchg['ADNAME'] = ad
	else:
		xchg['ADNAME'] = nameserver_ad		
	xchg['RHID'] = rhid
	xchg['HNAME'] = getHostname()
	
	xchg['EXTERNAL_IP'] = ip_interface[0][4]

	# Handle the pure XIA ports (0-2)
	i = 0  
	while i < 3 and i < len(xia_interfaces):  # Only go to 2 because port 3 is connected to IP
		repl = 'IP_ACTIVE' + str(i)
		xchg[repl] = str(0)
		repl = 'MAC' + str(i)
		xchg[repl] = xia_interfaces[i][1]
		repl = 'IPADDR' + str(i)
		xchg[repl] = xia_interfaces[i][2]
		repl = 'GWADDR' + str(i)
		xchg[repl] = xia_interfaces[i][3]
		repl = 'EXT_IPADDR' + str(i)
		xchg[repl] = xia_interfaces[i][4]
		i += 1
	while i < 3:
		repl = 'IP_ACTIVE' + str(i)
		xchg[repl] = str(0)
		repl = 'MAC' + str(i)
		xchg[repl] = "00:00:00:00:00:00"
		repl = 'IPADDR' + str(i)
		xchg[repl] = "1.1.1.1"
		repl = 'GWADDR' + str(i)
		xchg[repl] = "1.1.1.1"
		repl = 'EXT_IPADDR' + str(i)
		xchg[repl] = "1.1.1.1"
		i += 1

	# Handle the IP port (3)
	xchg['IP_ACTIVE3'] = str(1)
	xchg['MAC3'] = ip_interface[0][1];
	xchg['IPADDR3'] = ip_interface[0][2]
	xchg['GWADDR3'] = ip_interface[0][3]
	xchg['EXT_IPADDR3'] = ip_interface[0][4]
	 	
	newtext = tpl.substitute(xchg)

	i = 0
	tpl = Template(body)
	for (interface, mac, ip, gw, ext_ip) in xia_interfaces:
		xchg['IFACE'] = xia_interfaces[i][0]
		xchg['MAC'] = xia_interfaces[i][1]
		xchg['NUM'] = i
		i += 1

		newtext += tpl.substitute(xchg)

		if i >= 3:
			break

	xchg['IFACE'] = ip_interface[0][0]
	xchg['MAC'] = ip_interface[0][1]
	xchg['NUM'] = 3
	newtext += tpl.substitute(xchg)

	tpl = Template(extra)
	while i < 3:
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
	global dual_stack
	global adname
	global nameserver 
	global ip_override_addr
	global interface_filter
	global interface
	try:
		shortopt = "hr4ni:a:m:f:I:t"
		opts, args = getopt.getopt(sys.argv[1:], shortopt, 
			["help", "router", "host", "dual-stack", "nameserver", "id=", "ad=", "manual-address=", "interface-filter=", "host-interface="])
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
		elif o in ("-i", "--id"):
			hostname = a
		elif o in ("-r", "--router"):
			nodetype = "router"
		elif o in ("-t", "--host"):
			nodetype = "host"
		elif o in ("-4", "--dual-stack"):
			dual_stack = True
		elif o in ("-m", "--manual-address"):
			ip_override_addr = a
		elif o in ("-f", "--interface-filter"):
			interface_filter = a
		elif o in ("-I", "--host-interface"):
			interface = a
		elif o in ("-n", "--nameserver"):
			nameserver = "yes"			
		else:
		 	assert False, "unhandled option"

#
# display helpful information
#
def help():
	print """
usage: xconfig [-h] [-rt] [-4] [-n] [-i hostname] [-m ipaddr] [-f if_filter] [-I host-interface]
where:
  -h			: get help
  --help

  -i <name> 	 : set HID name tp <name>
  --id=<name>

  -a <name>		 : set AD name to <name>
  --ad=<name>

  -r			: do router config instead of host
  --router
  
  -t			: do a host config (this is the default)
  --host
  
  -4			: do a dual-stack config
  --dual-stack
  
  -n			: indicate that this needs to use nameserver AD and HID
  --nameserver  
  
  -m			: manually provide IP address
  --manual-address
  
  -f			: a CSV string; any interfaces whose names match one of the strings will be ignored
  --interface-filter=<filter string>
  
  -I			: the network interface a host should use, if it has multiple
  --host-interface=<interface>
"""
	sys.exit()

#
# do it
#
def main():

	getOptions()

	hid = createHID()
	rhid = createHID("20000000")
	makeXIAAddrConfig(hid)

	if (nodetype == "host"):
		if dual_stack:
			ad = createAD()
			makeDualHostConfig(ad, hid, rhid)
		else:
			makeHostConfig(hid)
	elif nodetype == "router":
		ad = createAD()
		if dual_stack:
			makeDualRouterConfig(ad, rhid)
		else:
			makeRouterConfig(ad, rhid)

if __name__ == "__main__":
	main()
