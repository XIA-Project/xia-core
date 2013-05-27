#!/usr/bin/python

import rpyc, time, threading, sys, curses, socket, thread
from rpyc.utils.server import ThreadedServer
from os.path import splitext
from timedthreadeddict import TimedThreadedDict
from check_output import check_output
from rpc import rpc
from random import choice
from subprocess import Popen, PIPE

RPC_PORT = 43278;
CLIENT_PORT = 3000
CHECK_PERIOD = 3
STATS_TIMEOUT = 3

HEARTBEATS = TimedThreadedDict() # hostname --> [color, lat, lon, [neighbor latlon]]
NEIGHBORD = TimedThreadedDict() # HID --> hostname
STATSD = {} # (hostname,hostname) --> [((hostname, hostname), backbone | test, ping, hops)]
LATLOND = {} # hostname --> [lat, lon]
NAMES = [] # names
NAME_LOOKUP = {} # names --> hostname
BACKBONES = [] # hostname
BACKBONE_TOPO = {} # hostname --> [hostname]
FINISHED_EVENT = threading.Event()
NUMEXP = 1
PLANETLAB_DIR = '/home/cmu_xia/fedora-bin/xia-core/experiments/planetlab/'
LOG_DIR = PLANETLAB_DIR + 'logs/'
STATS_FILE = LOG_DIR + 'stats-tunneling.txt'
            #f = open(LOGDIR+'stats-%s.txt' % splitext(sys.argv[1])[0].split('/')[-1], 'w')
LATLON_FILE = PLANETLAB_DIR + 'IPLATLON'
NAMES_FILE = PLANETLAB_DIR + 'names'
BACKBONE_TOPO_FILE = PLANETLAB_DIR + 'backbone_topo'

# note that killing local server is not in this one
STOP_CMD = '"sudo killall sh; sudo killall init.sh; sudo killall rsync; sudo ~/fedora-bin/xia-core/bin/xianet stop; sudo killall mapper_client.py"'
KILL_LS = '"sudo killall local_server.py"'
START_CMD = '"curl https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/init.sh > ./init.sh && chmod 755 ./init.sh && ./init.sh"'
SSH_CMD = 'ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 cmu_xia@'
XIANET_FRONT_CMD = 'until sudo ~/fedora-bin/xia-core/bin/xianet -v -r -P'
XIANET_BACK_CMD = '-f eth0 start; do echo "restarting click"; done\n'

CLIENTS = [] # hostname
CURRENT_EXP = ()

def stime():
    return time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())

class MasterService(rpyc.Service):
    def on_connect(self):
        self._host = socket.gethostbyaddr(self._conn._config['endpoints'][1][0])
        # code that runs when a connection is created
        # (to init the serivce, if needed)
        pass

    def on_disconnect(self):
        # code that runs when the connection has already closed
        # (to finalize the service, if needed)
        pass

    def exposed_heartbeat(self, hid, neighbors):
        lat = LATLOND[self._host][0]
        lon = LATLOND[self._host][1]
        NEIGHBORD[hid] = self._host;
        nlatlon = []
        for neighbor in neighbors:
            try:
                nlatlon.append(LATLOND[NEIGHBORD[neighbor]])
            except:
                pass
        color = 'red' if host in BACKBONES else 'blue'
        HEARTBEATS[self._host] = [color, lat, lon, nlatlon]

    def exposed_get_backbone(self):
        return BACKBONES

    def exposed_get_neighbor(self):
        neighbor = [client for client in CLIENTS if client is not self._host][0]
        return 'RE AD:%s HID:%s' % (rpc(neighbor, 'get_AD', ()), rpc(neighbor, 'get_hid', ()))

    def exposed_stats(self, backbone_name, ping, hops):
        try:
            STATSD[CURRENT_EXP].append(((self._host,backbone_name),'backbone',ping,hops))
        except:
            STATSD[CURRENT_EXP] = [((self._host,backbone_name),'backbone',ping,hops)]

        s = '%s:\t %s\n' % (key,STATSD[CURRENT_EXP])
        print s
        #stdscr.addstr(16, 0, s)

    def exposed_xstats(self, xping, xhops):
        STATSD[CURRENT_EXP].append((CURRENT_EXP,'test',xping,xhops))

        s = '%s:\t %s\n' % (key,STATSD[CURRENT_EXP])
        print s
        #stdscr.addstr(16, 0, s)

        if len(STATSD[CURRENT_EXP]) is 4:
            self.exposed_new_exp()

    def exposed_new_exp(self):
        global CLIENTS, NUMEXP
        [rpc(client, 'hard_stop', ()) for client in CLIENTS]

        client_a = choice(NAMES[11:])
        client_b = client_a
        while client_b is client_a:
            client_b = choice(NAMES[11:])

        CLIENTS = sorted([NAME_LOOKUP[client_a], NAME_LOOKUP[client_b]])
        CURRENT_EXP = tuple(CLIENTS)
        
        print '%s: new experiment (%s): %s' % (stime(), NUMEXP, CLIENTS)
        #stdscr.addstr(26, 0, '%s: new experiment (%s): %s' % (stime(), NUMEXP, CLIENTS))
        #[self.exposed_hard_restart(client) for client in CLIENTS]
        NUMEXP += 1

    def exposed_error(self, msg, host=None):
        host = self._host() if host is None else host
        try:
            rpc(host, 'hard_stop', ())
        except:
            pass
        if host in BACKBONES:
            self.exposed_hard_restart(host)
        print '%s: %s  (error!): %s' % (stime(), host, msg)
        #stdscr.addstr(30, 0, '%s: %s  (error!): %s' % (stime(), host, msg))
        self.exposed_new_exp()

    def exposed_hard_stop(self):
        [rpc(client, 'hard_stop', ()) for client in CLIENTS]
        [rpc(backbone, 'hard_stop', ()) for backbone in BACKBONES]
        sys.exit(-1)

    def ssh_run(self, host, cmd, check=True):
        if check is True:
            return check_output(SSH_CMD+'%s %s' % (host, cmd))
        else:
            return Popen(SSH_CMD+'%s %s' % (host, cmd), shell=True, stdout=PIPE, stderr=PIPE).communicate()[0]
            
    def launch_process(self, host):
        print 'launching...'
        #f = open('/tmp/%s-log' % (host),'w')
        try:
            rpc(host, 'hard_stop', ())
        except:
            pass
        self.ssh_run(host, STOP_CMD, check=False)
        self.ssh_run(host, KILL_LS, check=False)
        #f.write(self.ssh_run(host, STOP_CMD, check=False))
        #f.write(self.ssh_run(host, KILL_LS, check=False))

        try:
            self.ssh_run(host, START_CMD)
            #f.write(self.ssh_run(host, START_CMD))
        except Exception, e:
            print e
            #self.exposed_error('Startup', host=host)
        #f.close()

    def exposed_hard_restart(self, host):
        thread.start_new_thread(self.launch_process, (host, ))

    def exposed_get_xianet(self, neighbor_host = None, host = None):
        host = self._host() if host is None else host
        if host in CLIENTS:
            return "check_output('%s %s:%s %s')" % (XIANET_FRONT_CMD, socket.gethostbyname(neighbor_host), CLIENT_PORT, XIANET_BACK_CMD)
        elif host in BACKBONE:
            links = list(set([[tuple(sorted((backbone,neighbor))) for neighbor in BACKBONE_TOPO[backbone]] for backbone in BACKBONES]))
            neighbors = ['%s:500%s' % (hostd[''.join([socket.gethostbyname(n) for n in link if n is not host])][1],links.index(link)) for link in links if host in link]
            if neighbor_host:
                neighbors.append('%s:%s' % (neighbor_host, CLIENT_PORT))
            return "check_output('%s %s %s')" % (XIANET_FRONT_CMD, ','.join(neighbors[0:4]), XIANET_BACK_CMD)
        return ''

    def get_commands(self, host = None):
        host = self._host() if host is None else host
        if host in backbone:
            return self.exposed_get_xianet(host = host)
        elif host in CLIENTS:
            cmd = ["my_backbone = rpc('localhost', 'gather_stats', ())[1]"]
            cmd += ["rpc('localhost', 'wait_for_neighbor', (my_backbone, 'waiting for backbone: %s' % my_backbone))"]
            cmd += ["rpc(my_backbone, 'soft_restart', (host, ))"]
            cmd += ["rpc('localhost', 'gather_xstats', ())"]
        return ''

    def get_kill(self):
        return STOP_CMD
        
class Printer(threading.Thread):
    def __init__(self, goOnEvent):
        super(Printer, self).__init__()
        self.goOnEvent = goOnEvent

    def buildMap(self, beats):
        url = 'http://maps.googleapis.com/maps/api/staticmap?center=Kansas&zoom=4&size=640x400&maptype=roadmap&sensor=false'
        for beat in beats:
            if beat[0] != 'red':
                url += '&markers=color:%s|%s,%s' % (beat[0],beat[2],beat[1])
            else:
                url += '&markers=%s,%s' % (beat[2],beat[1])
            url += ''.join(['&path=%s,%s|%s,%s' % (beat[2],beat[1],x[1],x[0]) for x in beat[3]])
        html = '<html>\n<head>\n<title>Current Nodes In Topology</title>\n<meta http-equiv="refresh" content="5">\n</head>\n<body>\n<img src="%s">\n</body>\n</html>' % url
        return html

    def run(self):
        while self.goOnEvent.isSet():
            beats = HEARTBEATS.getClients()
            f = open('/var/www/html/map.html', 'w')
            f.write(self.buildMap(beats))
            f.close()
            beats = [beat[3] for beat in beats]
            print '%s : Active clients: %s\r' % (stime(), beats)
            #stdscr.addstr(0, 0, '%s : Active clients: %s\r' % (stime(), beats))

            s = ''
            for key, value in STATSD.iteritems():
                s += '%s:\t %s\n' % (key,value)
            f = open(STATS_FILE, 'w')
            f.write(s)
            f.close()
            print '%s : Writing out Stats' % stime()
            #stdscr.addstr(20, 0, '%s : Writing out Stats' % stime())

            #stdscr.refresh()
            #stdscr.clearok(1)
            
            time.sleep(CHECK_PERIOD)


class Runner(threading.Thread):
    def run(self):
        while True:
            try:
                rpc('localhost', 'hard_restart', (BACKBONES[0], ))
                print 'done rpc'
            except Exception, e:
                print e
                time.sleep(1)
                pass
            else:
                break
                
        #[rpc('localhost', 'hard_restart', (backbone, )) for backbone in BACKBONES]
        #rpc('localhost', 'new_exp', ())


if __name__ == '__main__':
    latlonfile = open(LATLON_FILE, 'r').read().split('\n')
    for ll in latlonfile:
        ll = ll.split(' ')
        LATLOND[ll[0]] = ll[1:-1]

    ns = open(NAMES_FILE,'r').read().split('\n')[:-1]
    NAMES = [n.split('#')[1].strip() for n in ns]
    nl = [line.split('#') for line in ns]
    NAME_LOOKUP = dict((n.strip(), host.strip()) for (host, n) in nl)

    BACKBONES = [NAME_LOOKUP[n.strip()] for n in NAMES[:11]]
    lines = open(BACKBONE_TOPO_FILE,'r').read().split('\n')
    for line in lines:
        BACKBONE_TOPO[NAME_LOOKUP[line.split(':')[0].strip()]] = tuple([l.strip() for l in line.split(':')[1].split(',')])

    print ('Threaded heartbeat server listening on port %d\n'
        'press Ctrl-C to stop\n') % RPC_PORT

    #stdscr = curses.initscr()
    #curses.noecho()
    #curses.cbreak()


    FINISHED_EVENT.set()
    printer = Printer(goOnEvent = FINISHED_EVENT)
    printer.start()

    runner = Runner()
    runner.start()

    try:
        t = ThreadedServer(MasterService, port = RPC_PORT)
        t.start()
    except Exception, e:
        print e

    #curses.echo()
    #curses.nocbreak()
    #curses.endwin()
    print 'Exiting, please wait...'
    FINISHED_EVENT.clear()
    printer.join()

    print 'Finished.'
    
    sys.exit(0)
