#!/usr/bin/python

UDP_PORT = 43278; CHECK_PERIOD = 3; CHECK_TIMEOUT = 15; STATS_TIMEOUT = 3

import rpyc, time, threading, sys, curses
from rpyc.utils.server import ThreadedServer

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

HEARTBEATS = TimedThreadedDict()
NEIGHBORD = TimedThreadedDict()
STATSD = {}
LATLOND = {}


class HeartBeatService(rpyc.Service):
    def on_connect(self):
        # code that runs when a connection is created
        # (to init the serivce, if needed)
        pass

    def on_disconnect(self):
        # code that runs when the connection has already closed
        # (to finalize the service, if needed)
        pass

    def exposed_heartbeat(self, ip, color, hid, neighbors):
        lat = LATLOND[ip][0]
        lon = LATLOND[ip][1]
        name = LATLOND[ip][2]
        NEIGHBORD[hid] = ip;
        nlatlon = []
        for neighbor in neighbors:
            try:
                nlatlon.append(LATLOND[NEIGHBORD[neighbor]])
            except:
                pass
        HEARTBEATS[ip] = [color, lat, lon, name, nlatlon]

    def exposed_stats(self, ip, backbone_name, ping, hops):
        STATSD[ip] = [(backbone_name,ping,hops)]
        stdscr.addstr(10, 0, '%s, %s, %s, %s' % (ip, backbone_name, ping, hops))

    def exposed_xstats(self, ip, neighbor_name, xping, xhops):
        STATSD[ip].append((neighbor_name,xping,xhops));
        stdscr.addstr(11, 0, '%s, %s, %s, %s' % (ip, neighbor_name, xping, xhops))

        
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

class Printer(threading.Thread):
    def __init__(self, goOnEvent):
        super(Printer, self).__init__()
        self.goOnEvent = goOnEvent

    def run(self):
        while self.goOnEvent.isSet():
            clients = HEARTBEATS.getClients()
            html = buildMap(clients)
            f = open('/var/www/html/map.html', 'w')
            f.write(html)
            f.close()
            clients = [client[3] for client in clients]
            stdscr.addstr(0, 0, '%s : Active clients: %s\r' % (time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime()), clients))
            
            stdout.write(curses.tigetstr("clear"))
            stdout.flush()
            stdscr.refresh()
            
            time.sleep(CHECK_PERIOD)

class DumpStats(threading.Thread):
    def __init__(self, goOnEvent):
        super(DumpStats, self).__init__()
        self.goOnEvent = goOnEvent

    def run(self):
        while self.goOnEvent.isSet():
            f = open('./stats.txt', 'w')
            for key, value in STATSD.iteritems():
                try:
                    f.write('%s:\t %s\n' % (HEARTBEATS[key][3],value))
                except:
                    f.write('%s:\t %s\n' % (key,value))
                    pass
            f.close()
            stdscr.addstr(20, 0, '%s : Writing out Stats' % (time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())))
            time.sleep(STATS_TIMEOUT)

if __name__ == '__main__':
    latlonfile = open('./mapper/IPLATLON', 'r').read().split('\n')
    for ll in latlonfile:
        ll = ll.split(' ')
        LATLOND[ll[0]] = ll[1:]


    print ('Threaded heartbeat server listening on port %d\n'
        'press Ctrl-C to stop\n') % UDP_PORT

    stdscr = curses.initscr()
    curses.noecho()
    curses.cbreak()

    finishEvent = threading.Event()
    finishEvent.set()
    printer = Printer(goOnEvent = finishEvent)
    printer.start()

    dumper = DumpStats(goOnEvent = finishEvent)
    dumper.start()


    t = ThreadedServer(HeartBeatService, port = UDP_PORT)
    t.start()
    curses.echo()
    curses.nocbreak()
    curses.endwin()
    print 'Exiting, please wait...'
    finishEvent.clear()
    printer.join()
    dumper.join()
    
    print 'Finished.'
