from ConfigParser import RawConfigParser
import genkeys

class XIAConfigReader:
    def __init__(self, config_filename, events_filename):
        self.control_addrs = {} # router: ctrl_addr
        self.router_ifaces = {} # router: [iface1,...]
        self.host_ifaces = {} # router: host_iface
        self.host_ipaddrs = {} # router: host ipaddr  # todo fill this
        self.route_info = {} # router: (dest,our_iface,their_iface,their_name)
        self.xcache = {} # router: runs_xcache
        self.nameserver = ""
        self.sid = {} # router : routing sid
        self.links = {}
        self.link_info = {}
 
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

            # IP Address of host iface
            self.host_ipaddrs[router] = parser.get(router, 'HostAddr')

            # Routing SID
            self.sid[router] = genkeys.create_new_SID()

            # Check if this is a nameserver
            try:
                if parser.getboolean(router, "NameServer"):
                    if len(self.nameserver) != 0:
                        print "ERROR: Only one NameServer allowed in config"
                        return
                    self.nameserver = router
            except:
                pass

            # Check if this router will have an Xcache
            self.xcache[router] = False
            try:
                if parser.getboolean(router, "Xcache"):
                    self.xcache[router] = True
            except:
                pass

            # Interface names for each router (comma separated list)
            interfaces = parser.get(router, 'Interfaces')
            interfaces = interfaces.replace(' ', '')
            self.router_ifaces[router] = interfaces.split(',')

            # Routes to other routers
            self.route_info[router] = []
            self.link_info[router] = {}
            self.links[router] = []
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
                self.links[router].append(dest)
                self.link_info[router][dest] = [our_iface, their_iface]

        #print self.control_addrs
        #print self.host_ifaces
        #print self.route_info
        #
        self.events = self.parse_events(events_filename)
        print self.events

    def parse_events(self, events_filename):

        parser = RawConfigParser()
        parser.read(events_filename)

        events = parser.sections()
        if len(events) == 0:
            print "ERROR: No sections found in events file"

        revents = {}
        for event in events:
            revents[event] = {}
            revents[event]['add_links'] = {}
            revents[event]['remove_links'] = {}
            revents[event]['delay'] = parser.get(event, 'delay')
            revents[event]['link_count'] = 0

            for name, val in parser.items(event):
                if name.startswith('add_'):
                    r = name.split('_')[1]
                    strippedval = val.replace(' ', '')
                    revents[event]['add_links'][r] = strippedval.split(',')
                    revents[event]['link_count'] += 1
                elif name.startswith('remove_'):
                    r = name.split('_')[1]
                    strippedval = val.replace(' ', '')
                    revents[event]['remove_links'][r] = strippedval.split(',')
                    revents[event]['link_count'] += 1

                else:
                    continue

        return revents

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
