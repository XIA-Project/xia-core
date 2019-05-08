from ConfigParser import RawConfigParser

class XIAConfigReader:
    def __init__(self, config_filename):
        self.control_addrs = {}
        self.host_ifaces = {}
        self.route_info = {}
        self.nameserver = ""

        # Read in the config file
        parser = RawConfigParser()
        parser.read(config_filename)

        # Router names are the section names in the config file
        routers = parser.sections()
        if len(routers) == 0:
            print "ERROR: No sections found in config file"

        # Read in info into our internal data structures
        for router in routers:

            # Control address for each router
            self.control_addrs[router] = parser.get(router, 'ControlAddress')

            # AID host interface for each router
            self.host_ifaces[router] = parser.get(router, 'HostInterface')

            # Check if this is a nameserver
            try:
                if parser.getboolean(router, "NameServer"):
                    if len(self.nameserver) != 0:
                        print "ERROR: Only one NameServer allowed in config"
                        return
                    self.nameserver = router
            except:
                pass

            # Routes to other routers
            self.route_info[router] = []
            for name, val in parser.items(router):
                if not name.startswith('route_'):
                    continue
                dest = name.split('_')[1]
                strippedval = val.replace(' ','')
                our_iface, them = strippedval.split('->')
                their_iface, their_name = them.split(':')
                #routename, iface = val.split(':')
                self.route_info[router].append(
                        (dest, our_iface, their_iface, their_name))
        #print self.control_addrs
        #print self.host_ifaces
        #print self.route_info

    def routers(self):
        return self.control_addrs.keys()

    def control_addr(self, router):
        return self.control_addrs[router]

    def host_iface(self, router):
        return self.host_ifaces[router]

    def route_list(self, router):
        return self.route_info[router]

    def is_nameserver(self, router):
        if router == self.nameserver:
            return True
        return False

if __name__ == "__main__":
    config = XIAConfigReader("demo.conf")
    for router in config.routers():
        print "{}, {}, nameserver:{}".format(router,
            config.control_addr(router), config.is_nameserver(router))
        print config.route_info[router]
