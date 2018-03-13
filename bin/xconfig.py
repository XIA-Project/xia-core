#! /usr/bin/env python2.7
# script to generate click host and router config files

import os
import re
import sys
import uuid
import getopt
import socket
import genkeys
import subprocess
import xiapyutils
from string import Template

# constants
templatedir = "etc/click/templates"
configdir = 'etc/click'
ext = 'template'
hostclick = 'host.click'
routerclick = 'router.click'
dualhostclick = 'dual_stack_host.click'
dualrouterclick = 'dual_stack_router.click'
if os.uname()[-1] == 'mips':
	ext = 'mips.template'

verbose = True

# Find where we are running out of
srcdir = xiapyutils.xia_srcdir()

# Update complete paths for template and config dirs
# NOTE: Must be done before calls to get_config/template_path functions
templatedir = os.path.join(srcdir, templatedir)
configdir = os.path.join(srcdir, configdir)

#
# Get the location of a template file
#
def get_template_path(click_file):
    return os.path.join(templatedir, '%s.%s' % (click_file, ext))

#
# Get the location for output config file
#
def get_config_path(click_file):
    return os.path.join(configdir, click_file)

# Template locations
hosttemplate = get_template_path(hostclick)
routertemplate = get_template_path(routerclick)
dualhosttemplate = get_template_path(dualhostclick)
dualroutertemplate = get_template_path(dualrouterclick)

# Output config file locations
hostconfig = get_config_path(hostclick)
routerconfig = get_config_path(routerclick)
dualhostconfig = get_config_path(dualhostclick)
dualrouterconfig = get_config_path(dualrouterclick)

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
remoteexec = False
dsrc_mac_addr = None
waveserver_ip = None
waveserver_port = None
socket_ips_ports = None
num_router_ports = 4

def message(msg, *params):
    if verbose:
        if params:
            print msg, params
        else:
            print msg

#
# create a globally unique HID based off of our mac address
#
def createHID():
    return genkeys.create_new_HID()

#
# create a globally unique AD based off of router's mac address
#
def createAD():
    return genkeys.create_new_AD()

#
# make a short hostname
#
def getHostname():
    global hostname
    if (len(hostname) == 0):
        hostname = xiapyutils.getxiaclickhostname()
    return hostname

#
# get list of network interfaces on OS X
#
def getOsxInterfaces(skip):

    addrs = []
    ignore = ''
    if skip != None:
        for filter in skip.split(','):
            ignore += 'grep -v %s | ' % filter.strip()
    use = ''

    cmd = "ifconfig | %s %s grep UP | cut -d: -f1" % (ignore, use)
    result = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
    interfaces = result.stdout.read().strip()
    interfaces = interfaces.split('\n')

    for interface in interfaces:
        cmd = 'ifconfig %s | grep ether | tr -s " " | cut -d\  -f2' % interface
        result = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
        mac = result.stdout.read().strip()
        cmd = 'ifconfig %s | grep "inet " | cut -d\  -f2' % interface
        result = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE)
        ip = result.stdout.read().strip()

        if mac != '' and ip != '':
            addrs.append([interface, mac, ip])

    return addrs

def mac_to_int(mac_str):
	return int(mac_str.replace(':', ''), 16)

def mac_str_from_ifconfig_out(ifconfig_out):
	match = re.search(r'([0-9A-F]{2}:){5}([0-9A-F]{2})', ifconfig_out, re.I)
	if match:
		return match.group()
	return None

def getRemoteInterfaces(ignore_interfaces):
    addrs = []
    # TODO: verify mac address is in correct format
    addrs.append(['wave0', dsrc_mac_addr, '0.0.0.0'])
    return addrs

def getAradaInterfaces(ignore_interfaces):
	addrs = []
	# Simply run ifconfig for wifi0
	wifi0_out = subprocess.check_output('ifconfig wifi0'.split())
	wifi0_mac_str = mac_str_from_ifconfig_out(wifi0_out)
	wifi0_mac_int = mac_to_int(wifi0_mac_str)
	addrs.append(['wifi0', wifi0_mac_str, '0.0.0.0'])

	# Get mac address of wifi1 on router to compare with wifi0
	try:
		wifi1_out = subprocess.check_output('ifconfig wifi1'.split())
		wifi1_mac_str = mac_str_from_ifconfig_out(wifi1_out)
		wifi1_mac_int = mac_to_int(wifi1_mac_str)
		if wifi1_mac_int < wifi0_mac_int:
			# Replace wifi0 entry in addrs with wifi1
			del addrs[:]
			addrs.append(['wifi1', wifi1_mac_str, '0.0.0.0'])
	except:
		# No wifi1 found, so return
		pass
	return addrs

#
# get list of interfaces on Linux
#
def getLinuxInterfaces(ignore_interfaces):
    # Get the MAC and IP addresses
    filters = ''
    if ignore_interfaces != None:
        filter_array = ignore_interfaces.split(",")
        for filter in filter_array:
            filters += 'grep -v %s | ' % filter.strip()

    cmdline = subprocess.Popen(
            "/sbin/ifconfig | %s grep -v fake | grep -A 1 HWaddr | sed 's/ \+/ /g' | sed s/addr://" % (filters),
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

    return addrs

#
# get a list of the interfaces and associated mac addresses on this machine
#
# ignore any interfaces matching 'filter' (e.g., eth0 on GENI or wlan0 on the
# demo Giadas as it is the control socket for the node) and also ignore any
# fake<n> interfaces as those are internal only
#
def getInterfaces(ignore_interfaces):

    # Get the default gateway  TODO: should there be one per interface?

    if os.uname()[0] == "Darwin":
        addrs = getOsxInterfaces(ignore_interfaces)
        cmd = "netstat -nr | grep -v : | grep default | tr -s ' ' | cut -d\  -f2"
    elif os.uname()[-1] == "mips":
        addrs = getAradaInterfaces(ignore_interfaces)
        cmd = "/sbin/route -n | grep ^0.0.0.0 | tr -s ' '  | cut -d ' ' -f2"

    elif remoteexec == True:
        addrs = getRemoteInterfaces(ignore_interfaces)
        cmd = "/sbin/route -n | grep ^0.0.0.0 | tr -s ' '  | cut -d ' ' -f2"

    else:
        addrs = getLinuxInterfaces(ignore_interfaces)
        cmd = "/sbin/route -n | grep ^0.0.0.0 | tr -s ' '  | cut -d ' ' -f2"

    cmdline = subprocess.Popen(cmd,
            shell=True, stdout=subprocess.PIPE)
    default_gw = cmdline.stdout.read().strip()

    interfaces = []
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
# Fill in the host template file
#
def makeHostConfig():

    interfaces = getInterfaces(interface_filter)

    if (len(interfaces) == 0) and not socket_ips_ports:
        print "no available interface"
        exit(1)
    elif (len(interfaces) > 1):
        message("multiple interfaces found, using " + interfaces[0][0])

    try:
        f = open(hosttemplate, "r")
        text = f.read()
        f.close()

        f = open(hostconfig, "w")

    except Exception, e:
        print "error opening file for reading and/or writing\n%s" % e
        sys.exit(-1)

    (header, socks, body, footer) = text.split("######")

    xchg = {}
    xchg['HNAME'] = getHostname()
    xchg['ADNAME'] = adname

    if (nameserver != "no"):
        xchg['HID'] = nameserver_hid

    if socket_ips_ports:
        ip, port = socket_ips_ports.split(',')[0].strip().split(':')
        xchg['PORT'] = int(port)
        xchg['SOCK_IP'] = ip
        xchg['CLIENT'] = 'false' if ip == '0.0.0.0' else 'true'
        text = header + socks + footer
    else:
        text = header + body + footer

    if interfaces:
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

def makeRouterConfig():
    makeRouterConfigToFile(routertemplate, routerconfig)

def makeRouterConfigToFile(template, outfile):
    global interface_filter

    interfaces = getInterfaces(interface_filter)
    message("Got these interfaces:", interfaces)

    socket_ip_port_list = [p.strip() for p in socket_ips_ports.split(',')] if socket_ips_ports else []
    message("Socket ip port list contains:", socket_ip_port_list)

    makeGenericRouterConfig(num_router_ports, socket_ip_port_list, interfaces, [], template, outfile)


# makeRouterConfig and makeDualRouterConfig call this
def makeGenericRouterConfig(num_ports, socket_ip_port_list, xia_interfaces, ip_interfaces, template, outfile):
    dummy_ip = '0.0.0.0'
    dummy_mac = '00:00:00:00:00:00'

    message("Opening template and output click file")
    try:
        f = open(template, 'r')
        text = f.read()
        f.close()
    except Exception, e:
        print 'error opening template file for reading:\n%s' % e
        sys.exit(-1)

    (header, socks_interfaces, raw_interfaces, unused_interfaces, footer) = text.split("######")

    # Assign ports to one of:
    # socket port -- uses click socket element to connect to other end of "wire"
    # raw port -- uses FromDevice/ToDevice to connect to other end
    # ip port -- ip speaking "raw" port on dual stack router
    # unused -- connected to click's Idle and Discard
    start_unassigned = 0
    end_unassigned = num_ports - 1

    # store the port numbers (0, 1, 2, etc) assigned to each type
    socket_ports = []
    raw_ports = []
    ip_ports = []
    unused_ports = []

    # map port numbers to interfaces (e.g., 0 -> eth0)
    interfaces = {}

    for ip_face in ip_interfaces:
        if start_unassigned > end_unassigned:
            break
        ip_ports.append(end_unassigned)
        interfaces[end_unassigned] = ip_face
        end_unassigned -= 1

    for ip_port_pair in socket_ip_port_list:
        if start_unassigned > end_unassigned:
            break
        socket_ports.append(start_unassigned)
        interfaces[start_unassigned] = (ip_port_pair, dummy_mac, dummy_ip, dummy_ip)  # ip_port_pair is a string: e.g. '1.2.3.4:4000'
        start_unassigned += 1

    for xia_face in xia_interfaces:
        if start_unassigned > end_unassigned:
            break
        raw_ports.append(start_unassigned)
        interfaces[start_unassigned] = xia_face
        start_unassigned += 1

    while start_unassigned <= end_unassigned:
        unused_ports.append(start_unassigned)
        interfaces[start_unassigned] = ('dummy', dummy_mac, dummy_ip, dummy_ip)
        start_unassigned += 1



    ##
    ## HEADER
    ##
    tpl = Template(header)

    xchg = {}

    xchg['PORTS'] = num_ports

    xchg['HNAME'] = getHostname()
    if remoteexec:
        xchg['RIPADDR'] = waveserver_ip
        xchg['RPORT'] = waveserver_port

    if len(ip_interfaces) > 0:
        xchg['EXTERNAL_IP'] = ip_interfaces[0][4]
    elif ip_override_addr == None:
        xchg['EXTERNAL_IP'] = dummy_ip
    else:
        xchg['EXTERNAL_IP'] = ip_override_addr

    # create $MAC0 thru $MACn replacements
    # create $IP_ACTIVE, $IPADDR, and $GWADDR replacements also,
    # though if this isn't a dual stack router, the strings we're
    # trying to replace won't exist in the template, but that's OK
    mac_list = ""
    for i in range(0, num_ports):
        mac_list = mac_list + interfaces[i][1] + ", "
        # xchg['MAC' + str(i)] = interfaces[i][1]
        xchg['IP_ACTIVE' + str(i)] = str(1) if i in ip_ports else str(0)
        xchg['IPADDR' + str(i)] = interfaces[i][2]
        xchg['GWADDR' + str(i)] = interfaces[i][3]
    xchg['MACS'] = mac_list[:-2]

    newtext = tpl.substitute(xchg)


    ##
    ## SOCKS
    ## (The router ports connected using click socket elements)
    ##
    tpl = Template(socks_interfaces)
    for i in socket_ports:
        ip, port = interfaces[i][0].split(':')

        xchg['SOCK_IP'] = ip
        xchg['PORT'] = port
        xchg['CLIENT'] = 'false' if ip == '0.0.0.0' else 'true'
        xchg['NUM'] = i

        newtext += tpl.substitute(xchg)

    ##
    ## RAW
    ## (The router ports connected to FromDevice/ToDevice)
    ##
    tpl = Template(raw_interfaces)
    for i in raw_ports + ip_ports:
        xchg['IFACE'] = interfaces[i][0]
        xchg['MAC'] = interfaces[i][1]
        xchg['NUM'] = i

        newtext += tpl.substitute(xchg)

    ##
    ## UNUSED
    ## (The router ports connected to Idle/Discard)
    ##
    tpl = Template(unused_interfaces)
    for i in unused_ports:
        xchg['NUM'] = i
        newtext += tpl.substitute(xchg)

    ##
    ## FOOTER
    ##
    tpl = Template(footer)
    newtext += tpl.substitute(xchg)


    # Write the generated conf to disk
    f = open(outfile, 'w')
    f.write(newtext)
    f.close()



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

def makeDualHostConfig(ad):
    global interface_filter, interface

    if interface_filter == None:
        filter = interface
    else:
        filter = '%s,%s' % (interface, interface_filter)

    ip_interface = getInterfaces(None, interface)
    xia_interfaces = getInterfaces(filter, None)

    try:
        f = open(dualhosttemplate, "r")
        text = f.read()
        f.close()

        f = open(dualhostconfig, "w")

    except:
        print "error opening file for reading and/or writing"
        sys.exit(-1)

    (header, socks, body, extra, footer) = text.split("######")

    tpl = Template(header)

    xchg = {}
    if (nameserver == "no"):
        xchg['ADNAME'] = ad
    else:
        xchg['ADNAME'] = nameserver_ad
        xchg['HID'] = nameserver_hid
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
    ##
    ## SOCKS
    ## (The router ports connected using click socket elements)
    ##
    if socket_ips_ports:
        tpl = Template(socks)
        for pair in [p.strip() for p in socket_ips_ports.split(',')]:
            if i > 4: # Make sure we go up to at most 4 ports
                break

            ip, port = pair.split(':')

            xchg['SOCK_IP'] = ip
            xchg['PORT'] = int(port)
            xchg['CLIENT'] = 'false' if ip == '0.0.0.0' else 'true'
            xchg['NUM'] = i
            i += 1

            newtext += tpl.substitute(xchg)


    ##
    ## BODY
    ## (The router ports connected to FromDevice/ToDevice)
    ##
    tpl = Template(body)
    start = i
    for (interface, mac, ip, gw, ext_ip) in xia_interfaces:
        if i > 3: # Make sure we go up to at most 4 ports
            break
        xchg['IFACE'] = xia_interfaces[i][0]
        xchg['MAC'] = xia_interfaces[i][1]
        xchg['NUM'] = i
        i += 1

        newtext += tpl.substitute(xchg)

    xchg['IFACE'] = ip_interface[0][0]
    xchg['MAC'] = ip_interface[0][1]
    xchg['NUM'] = 3
    newtext += tpl.substitute(xchg)

    ##
    ## EXTRA
    ##
    tpl = Template(extra)
    while i < 2:
        xchg['NUM'] = i
        newtext += tpl.substitute(xchg)
        i += 1

    ##
    ## FOOTER
    ##
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

def makeDualRouterConfig(ad):
    global interface_filter, interface

    if interface == None:
        print "Must specify which interface should be this router's IP-speaking port"
        sys.exit(1)


    if interface_filter == None:
        filter = interface
    else:
        filter = '%s,%s' % (interface, interface_filter)

    ip_interfaces = getInterfaces(None, interface)
    xia_interfaces = getInterfaces(filter, None)

    socket_ip_port_list = [p.strip() for p in socket_ips_ports.split(',')] if socket_ips_ports else []

    makeGenericRouterConfig(4, ad, socket_ip_port_list, xia_interfaces, ip_interfaces, dualroutertemplate, dualrouterconfig)


#
# parse the command line so we can do stuff
#
def getOptions(cmdline):
    global hostname
    global nodetype
    global dual_stack
    global adname
    global nameserver
    global ip_override_addr
    global interface_filter
    global interface
    global remoteexec
    global dsrc_mac_addr
    global waveserver_ip
    global waveserver_port
    global ext
    global hostclick
    global routerclick
    global hosttemplate
    global routertemplate
    global socket_ips_ports
    global num_router_ports
    try:
        shortopt = "hr48ni:a:m:f:I:W:tP:"
        #opts, args = getopt.getopt(sys.argv[1:], shortopt,
        opts, args = getopt.getopt(cmdline, shortopt,
            ["help", "router", "host", "dual-stack", "nameserver", "manual-address=", "interface-filter=", "host-interface=", "waveserver=", "socket-ports="])
    except getopt.GetoptError, err:
        # print     help information and exit:
        print str(err) # will print something like "option -a not recognized"
        help()
        sys.exit(2)

    for o, a in opts:
        if o in ("-h", "--help"):
            help()
        elif o in ("-i", "--id"):
            hostname = a
        elif o in ("-r", "--router"):
            nodetype = "router"
        elif o in ("-t", "--host"):
            nodetype = "host"
        elif o in ("-4", "--dual-stack"):
            dual_stack = True
        elif o in ("-8"):
            num_router_ports = 8
        elif o in ("-m", "--manual-address"):
            ip_override_addr = a
        elif o in ("-f", "--interface-filter"):
            interface_filter = a
        elif o in ("-I", "--host-interface"):
            interface = a
        elif o in ("-W", "--waveserver"):
            remoteexec = True
            ext = 'remote.template'
            hosttemplate = get_template_path(hostclick)
            routertemplate = get_template_path(routerclick)
            dsrc_mac_addr, waveserver_addr = a.split(',')
            waveserver_ip, waveserver_port = waveserver_addr.split(':')
        elif o in ("-P", "--socket-ports"):
            socket_ips_ports = a
        elif o in ("-n", "--nameserver"):
            nameserver = "yes"
        else:
             assert False, "unhandled option %s" % o

#
# display helpful information
#
def help():
    print """
usage: xconfig [-h] [-rt] [-4] [-8] [-n] [-i hostname] [-m ipaddr] [-f if_filter] [-P socket-ports] [-I host-interface] [-W <dsrc_mac_addr>,<arada_ip_addr>:<waveserver_port_num>]
where:
  -h            : get help
  --help

  -i <name>      : set HID name tp <name>
  --id=<name>

  -r            : do router config instead of host
  --router

  -t            : do a host config (this is the default)
  --host

  -4            : do a dual-stack config
  --dual-stack

  -8            : create an 8 port router (with -r)

  -m            : manually provide IP address
  --manual-address

  -f            : a CSV string; any interfaces whose names match one of the strings will be ignored
  --interface-filter=<filter string>

  -P            : a CSV string; the IP addr/TCP port pairs to be used to tunnel packets between click sockets. Each pair should be colon-separated: <ip>:<port>
  --socket-ports=<ports>

  -I            : the network interface a host should use, if it has multiple
  --host-interface=<interface>

  -W            : DSRC Interface mac addr, addr:port of waveserver on Arada box
  --waveserver=<dsrc_mac_addr>,<arada_ip_addr>:<waveserver_port_num>
"""
    sys.exit()

#
# do it
#
def main():
    build(sys.argv[1:])


def build(cmds):
    getOptions(cmds)

    message("Checking type of node to create")
    if (nodetype == "host"):
        message("Creating host")
        if dual_stack:
            message("Creating dual stack host")
            ad = createAD()
            makeDualHostConfig(ad)
        else:
            message("Calling makeRouterConfig")
            makeRouterConfigToFile(hosttemplate, hostconfig)
    elif nodetype == "router":
        ad = createAD()
        if dual_stack:
            makeDualRouterConfig(ad)
        else:
            makeRouterConfig()

if __name__ == "__main__":
    main()
