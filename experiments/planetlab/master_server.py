#!/usr/bin/python

UDP_PORT = 43278; CHECK_PERIOD = 3; CHECK_TIMEOUT = 6

import rpyc, time

class TimedThreadedDict(dict):
    """Manage shared heartbeats dictionary with thread locking"""

    def __init__(self):
        super(TimedThreadedDict, self).__init__()
        self._lock = threading.Lock()

    def __setitem__(self, key, value):
        """Create or update the dictionary entry for a client"""
        if isinstance(value, list):
            timeval = [time.time()] + value
        else:
            timeval = [time.time(), value]
        self._lock.acquire()
        dict.__setitem__(self, key, timeval)
        self._lock.release()
        
    def __getitem__(self, key):
        limit = time.time() - CHECK_TIMEOUT
        val =  dict.__getitem__(self, key)
        if val[0] >= limit:
            if len(val) == 2:
                return val[1]
            return val[1:]
        raise Exception("KeyError: " + key)            

    def getClients(self):
        limit = time.time() - CHECK_TIMEOUT
        self._lock.acquire()
        clients = [val[1:] for (ip, val) in self.items() if val[0] >= limit]
        self._lock.release()
        return clients

class HeartBeatService(rpyc.Service):
    def __init__(self, heartbeats, neighbord, statsd):
        super(HeartBeatService, self).__init__()
        self.heartbeats = heartbeats
        self.neighbord = neighbord
        self.statsd = statsd
        self.latlonfile = open('./mapper/IPLATLON', 'r').read().split('\n')
        self.latlond = {}
        for ll in self.latlonfile:
            ll = ll.split(' ')
            self.latlond[ll[0]] = ll[1:]

    def on_connect(self):
        # code that runs when a connection is created
        # (to init the serivce, if needed)
        pass

    def on_disconnect(self):
        # code that runs when the connection has already closed
        # (to finalize the service, if needed)
        pass

    def exposed_heartbeat(self, ip, color, hid, neighbors):
        lat = self.latlond[ip][0]
        lon = self.latlond[ip][1]
        name = self.latlond[ip][2]
        self.neighbord[hid] = ip;
        nlatlon = []
        for neighbor in neighbors:
            try:
                nlatlon.append(self.latlond[self.neighbord[neighbor]])
            except:
                pass
        self.heartbeats[ip] = [color, lat, lon, name, nlatlon]

    def exposed_stats(self, ip, name, ping, hops):
        self.statsd[ip] = (name,ping,hops)
        print stats

        
def buildMap(clients):
    url = 'http://maps.googleapis.com/maps/api/staticmap?center=Kansas&zoom=4&size=640x400&maptype=roadmap&sensor=false'
    for client in clients:
        if client[0] != 'red':
            url += '&markers=color:%s|%s,%s' % (client[0],client[2],client[1])
        else:
            url += '&markers=%s,%s' % (client[2],client[1])
        url += ''.join(['&path=%s,%s|%s,%s' % (client[2],client[1],x[1],x[0]) for x in client[4]])
    html = '<html>\n<head>\n<title>Current Nodes In Topology</title>\n<meta http-equiv="refresh" content="3">\n</head>\n<body>\n<img src="%s">\n</body>\n</html>' % url
    return html

def main():
    heartbeats = TimedThreadedDict()
    neighbord = TimedThreadedDict()
    statsd = {}
    receiver = HeartBeatService(heartbeats = heartbeats, neighbord = neighbord, statsd = statsd)
    t = ThreadedServer(MyService, port = UDP_PORT)
    print ('Threaded heartbeat server listening on port %d\n'
        'press Ctrl-C to stop\n') % UDP_PORT
    try:
        while True:
            clients = heartbeats.getClients()
            html = buildMap(clients)
            f = open('/var/www/html/map.html', 'w')
            f.write(html)
            f.close()
            clients = [client[3] for client in clients]
            print 'Active clients: %s' % clients
            time.sleep(CHECK_PERIOD)
    except KeyboardInterrupt:
        print 'Exiting, please wait...'
        receiverEvent.clear()
        receiver.join()
        print 'Finished.'

if __name__ == '__main__':
    main()
