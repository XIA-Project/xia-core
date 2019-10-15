
from ConfigParser import RawConfigParser

class XIAClientConfigReader:
    def __init__(self, config_filename):
       self.routers = {}
       self.default_router = {}

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

           self.default_router[client] = parser.get(client, 'Default')

    def clients(self):
        return self.routers.keys()



if __name__ == "__main__":
    config = XIAClientConfigReader("client.conf")
    for client in config.clients():
        print client + ':'
        print config.routers[client]