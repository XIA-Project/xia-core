#!/usr/bin/python

UDP_PORT = 43278; CHECK_PERIOD = 3; CHECK_TIMEOUT = 15; STATS_TIMEOUT = 3

import rpyc, time, threading, sys, curses
from subprocess import call, PIPE
from rpyc.utils.server import ThreadedServer
from os.path import splitext

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
finishEvent = threading.Event()
NUMEXP = 0

def get_stats():
    s = ''
    for key, value in STATSD.iteritems():
        s += '%s:\t %s\n' % (key,value)
    return s



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

    def exposed_stats(self, my_name, neighbor_name, backbone_name, ping, hops):
        key = tuple(sorted([my_name,neighbor_name]))
        try:
            STATSD[key].append(((my_name,backbone_name),'backbone',ping,hops))
        except:
            STATSD[key] = [((my_name,backbone_name),'backbone',ping,hops)]
        s = '%s:\t %s\n' % (key,STATSD[key])
        stdscr.addstr(16, 0, s)

    def exposed_xstats(self, my_name, neighbor_name, xping, xhops):
        key = tuple(sorted([my_name,neighbor_name]))
        STATSD[key].append(((my_name,neighbor_name),'test',xping,xhops))
        s = '%s:\t %s\n' % (key,STATSD[key])
        stdscr.addstr(16, 0, s)
        if len(STATSD[key]) is 4:
            self.exposed_newtopo()

    def exposed_newtopo(self):
        global NUMEXP
        clients = open(sys.argv[1]).read().split('[clients]')[1].split('\n')
        clients = [client.split(':')[0].strip() for client in clients]
        clients = clients[1:]
        outs = [call('./run.py %s stop %s' % (sys.argv[1], client),shell=True, stdout=PIPE, stderr=PIPE) for client in clients]
        out = call('./generate_topo.py',shell=True, stdout=PIPE, stderr=PIPE)

        clients = open(sys.argv[1]).read().split('[clients]')[1].split('\n')
        clients = [client.split(':')[0].strip() for client in clients]
        clients = clients[1:]
        stdscr.addstr(26, 0, '%s: new experiment (%s): %s' % (time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime()), NUMEXP, clients))
        out = [call('./run.py %s start %s' % (sys.argv[1], client),shell=True, stdout=PIPE, stderr=PIPE) for client in clients]
        NUMEXP += 1
        return 'done'

    def exposed_stop(self):
        out = call('./run.py %s stop' % sys.argv[1],shell=True, stdout=PIPE, stderr=PIPE)
        return 'done'
        #finishEvent.clear()
        
def buildMap(clients):
    url = 'http://maps.googleapis.com/maps/api/staticmap?center=Kansas&zoom=4&size=640x400&maptype=roadmap&sensor=false'
    for client in clients:
        if client[0] != 'red':
            url += '&markers=color:%s|%s,%s' % (client[0],client[2],client[1])
        else:
            url += '&markers=%s,%s' % (client[2],client[1])
        url += ''.join(['&path=%s,%s|%s,%s' % (client[2],client[1],x[1],x[0]) for x in client[4]])
    html = '<html>\n<head>\n<title>Current Nodes In Topology</title>\n<meta http-equiv="refresh" content="5">\n</head>\n<body>\n<img src="%s">\n</body>\n</html>' % url
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

            stdscr.refresh()
            stdscr.clearok(1)
            
            time.sleep(CHECK_PERIOD)

class DumpStats(threading.Thread):
    def __init__(self, goOnEvent):
        super(DumpStats, self).__init__()
        self.goOnEvent = goOnEvent

    def run(self):
        while self.goOnEvent.isSet():
            try:
                f = open('./stats-%s.txt' % splitext(sys.argv[1])[0].split('/')[-1], 'w')
                f.write(get_stats())
                f.close()
                stdscr.addstr(20, 0, '%s : Writing out Stats' % (time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())))
            except:
                pass
            time.sleep(STATS_TIMEOUT)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print 'usage %s [topo_file]' % (sys.argv[0])
        sys.exit(-1)


    latlonfile = open('./mapper/IPLATLON', 'r').read().split('\n')
    for ll in latlonfile:
        ll = ll.split(' ')
        LATLOND[ll[0]] = ll[1:]


    print ('Threaded heartbeat server listening on port %d\n'
        'press Ctrl-C to stop\n') % UDP_PORT

    stdscr = curses.initscr()
    curses.noecho()
    curses.cbreak()


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
