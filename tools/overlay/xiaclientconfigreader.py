

from ConfigParser import RawConfigParser

class XIAClientConfigReader:
    def __init__(self, config_filename):
       #client metadata
       self.routers = {}
       self.default_router = {}
       self.control_addr = {}
       self.control_port = {}
       #router metadata
       self.router_iface = {}
       self.router_addr = {}
       self.ad = {}
       self.hid = {}

       # Read in the config file
       parser = RawConfigParser()
       parser.read(config_filename)

       # Router names are the section names in the config file
       clients = parser.sections()
       if len(clients) == 0:
          print "ERROR: No sections found in config file"

       # Read in info into our internal data structures
       for client in clients:

           # Interface names for each router (comma separated list)
           routers = parser.get(client, 'Routers')
           routers = routers.replace(' ', '')
           self.routers[client] = routers.split(',')

           ifaces = parser.get(client, 'Interfaces')
           ifaces = ifaces.replace(' '. '')
           self.router_iface[client] = []
           r_iface = []
           for i, iface in ifaces:
              r_iface[routers[i]] = iface
           self.router_iface[client] = r_iface

           self.default_router[client] = parser.get(client, 'Default')
           self.control_addr[client] = parser.get(client, 'ControlAddress')
           self.control_port[client] = parser.get(client, 'ControlPort')


    def clients(self):
        return self.routers.keys()


if __name__ == "__main__":
    config = XIAClientConfigReader("client.conf")
    for client in config.clients():
        print client + ':'
        print config.routers[client]
