#!/usr/bin/python

import rpyc, time, threading, sys, curses, socket, thread
from threading import Thread
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
LATLOND = {} # IP --> [lat, lon]
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
STOP_CMD = '"sudo killall sh; sudo killall init.sh; sudo killall rsync; sudo ~/fedora-bin/xia-core/bin/xianet stop; sudo killall mapper_client.py; sudo killall xping; sudo killall xtraceroute"'
KILL_LS = '"sudo killall local_server.py; sudo killall python"'
START_CMD = '"curl https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/init.sh > ./init.sh && chmod 755 ./init.sh && ./init.sh"'
SSH_CMD = 'ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 cmu_xia@'
XIANET_FRONT_CMD = 'until sudo ~/fedora-bin/xia-core/bin/xianet -v -r -P'
XIANET_BACK_CMD = '-f eth0 start; do echo "restarting click"; done'

CLIENTS = [] # hostname

def stime():
    return time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())

def std_listen(handle, out):
    while True:
        line = handle.readline()
        if not line:
            return
        if out: out.write(line)

def ssh_run(host, cmd, out, check=True):
    c = SSH_CMD+'%s %s' % (host, cmd)
    print '%s: launching subprocess: %s' % (host, cmd)
    if out: out.write('launching subprocess: %s' % cmd)
    p = Popen(SSH_CMD+'%s %s' % (host, cmd), shell=True, stdout=PIPE, stderr=PIPE)
    thread.start_new_thread(std_listen, (p.stdout, out))
    thread.start_new_thread(std_listen, (p.stderr, out))
    p.wait()
    print '%s: finished running subprocess: %s' % (host, cmd)
    if out: out.write('finished running subprocess: %s' % cmd)
    if check is True:
        rc = p.returncode
        if rc is not 0:
            raise Exception("subprocess.CalledProcessError: Command '%s'" \
                                "returned non-zero exit status %s" % (c, rc))


class MasterService(rpyc.Service):
    def on_connect(self):
        self._host = socket.gethostbyaddr(self._conn._config['endpoints'][1][0])[0].lower()
        self._conn._config['allow_pickle'] = True
        # code that runs when a connection is created
        # (to init the serivce, if needed)
        pass

    def on_disconnect(self):
        # code that runs when the connection has already closed
        # (to finalize the service, if needed)
        pass

    def exposed_heartbeat(self, hid, neighbors):
        lat, lon = LATLOND[socket.gethostbyname(self._host)]
        NEIGHBORD[hid] = self._host;
        nlatlon = []
        for neighbor in neighbors:
            try:
                nlatlon.append(LATLOND[socket.gethostbyname(NEIGHBORD[neighbor])])
            except:
                pass
        color = 'red' if self._host in BACKBONES else 'blue'
        HEARTBEATS[self._host] = [color, lat, lon, nlatlon]

    def exposed_get_backbone(self):
        return BACKBONES

    def exposed_get_neighbor_xhost(self):
        if self._host in CLIENTS:
            neighbor = [client for client in CLIENTS if client != self._host][0]
            return 'RE AD:%s HID:%s' % (rpc(neighbor, 'get_ad', ()), rpc(neighbor, 'get_hid', ()))
        return None

    def exposed_get_neighbor_host(self):
        if self._host in CLIENTS:
            return [client for client in CLIENTS if client != self._host][0]

    def exposed_stats(self, backbone_name, ping, hops):        
        cur_exp = tuple(CLIENTS)
        try:
            STATSD[cur_exp].append(((self._host,backbone_name),'backbone',ping,hops))
        except:
            STATSD[cur_exp] = [((self._host,backbone_name),'backbone',ping,hops)]

        s = '%s:\t %s\n' % (cur_exp,STATSD[cur_exp])
        print s
        #stdscr.addstr(16, 0, s)

    def exposed_xstats(self, xping, xhops):
        cur_exp = tuple(CLIENTS)
        STATSD[cur_exp].append((cur_exp,'test',xping,xhops))

        s = '%s:\t %s\n' % (cur_exp,STATSD[cur_exp])
        print s
        #stdscr.addstr(16, 0, s)

        #if len(STATSD[cur_exp]) is 4:
            #self.exposed_new_exp()

    def exposed_new_exp(self):
        global CLIENTS, NUMEXP
        [rpc(client, 'hard_stop', ()) for client in CLIENTS]

        client_a = choice(NAMES[11:])
        client_b = client_a
        while client_b == client_a:
            client_b = choice(NAMES[11:])

###########
        client_a = 'planetlab1.tsuniv.edu'
        client_b = 'planetlab5.cs.cornell.edu'
##########

        CLIENTS = sorted([NAME_LOOKUP[client_a], NAME_LOOKUP[client_b]])
        
        print '%s: new experiment (%s): %s' % (stime(), NUMEXP, CLIENTS)
        #stdscr.addstr(26, 0, '%s: new experiment (%s): %s' % (stime(), NUMEXP, CLIENTS))
        [self.exposed_hard_restart(client) for client in CLIENTS]
        NUMEXP += 1

    def exposed_error(self, msg, host=None):
        return

    ########
        host = self._host if host == None else host
        try:
            rpc(host, 'hard_stop', ())
        except:
            pass
        if host in BACKBONES:
            self.exposed_hard_restart(host)
        print '%s: %s  (error!): %s' % (stime(), host, msg)
        #stdscr.addstr(30, 0, '%s: %s  (error!): %s' % (stime(), host, msg))
        #self.exposed_new_exp()

    def exposed_hard_stop(self):
        [rpc(client, 'hard_stop', ()) for client in CLIENTS]
        [rpc(backbone, 'hard_stop', ()) for backbone in BACKBONES]
        sys.exit(-1)
            
    def launch_process(self, host):
        print '%s: launching...' % host
        f = open('/tmp/%s-log' % (host),'w',0)
        f.write('launching...\n')
        try:
            rpc(host, 'hard_stop', ())
        except:
            pass
        ssh_run(host, STOP_CMD, f, check=False)
        ssh_run(host, KILL_LS, f, check=False)

        try:
            ssh_run(host, START_CMD, f)
        except Exception, e:
            print e
            #self.exposed_error('Startup', host=host)
        print '%s: finished running process' % host
        f.write('finished running process')
        f.close()

    def exposed_hard_restart(self, host):
        thread.start_new_thread(self.launch_process, (host, ))

    def exposed_get_xianet(self, neighbor_host = None, host = None):
        host = self._host if host == None else host
        if host in CLIENTS:
            return "check_output('%s %s:%s %s')" % (XIANET_FRONT_CMD, socket.gethostbyname(neighbor_host), CLIENT_PORT, XIANET_BACK_CMD)
        elif host in BACKBONES:
            links = list(set([tuple(sorted((backbone,neighbor))) for backbone in BACKBONES for neighbor in BACKBONE_TOPO[backbone]]))
            neighbors = ['%s:500%s' % ([socket.gethostbyname(n) for n in link if n != host][0],links.index(link)) for link in links if host in link]
            if neighbor_host:
                neighbors.append('%s:%s' % (socket.gethostbyname(neighbor_host), CLIENT_PORT))
            return "check_output('%s %s %s')" % (XIANET_FRONT_CMD, ','.join(neighbors[0:4]), XIANET_BACK_CMD)
        return ''

    def exposed_get_commands(self, host = None):
        host = self._host if host == None else host
        print 'MASTER: %s checked in for commands' % host
        if host in BACKBONES:
            return [self.exposed_get_xianet(host = host)]
        elif host in CLIENTS:
            cmd = ["my_backbone = rpc('localhost', 'gather_stats', ())[1]"]
            cmd += ["rpc('localhost', 'wait_for_neighbor', (my_backbone, 'waiting for backbone: %s' % my_backbone))"]
            cmd += ["rpc(my_backbone, 'soft_restart', (my_name, ))"]
            cmd += ["xianetcmd = rpc(MASTER_SERVER, 'get_xianet', (my_backbone, ))"]
            cmd += ["print xianetcmd"]
            cmd += ["thread.start_new_thread(exec_fun, (xianetcmd, ))"]
            cmd += ["rpc('localhost', 'wait_for_neighbor', ('localhost', 'waiting for xianet to start'))"]
            cmd += ["my_neighbor = rpc(MASTER_SERVER, 'get_neighbor_host', ())"]
            cmd += ["print my_neighbor"]
            cmd += ["rpc('localhost', 'wait_for_neighbor', (my_backbone, 'waiting for backbone: %s' % my_backbone))"]
            cmd += ["rpc('localhost', 'wait_for_neighbor', (my_neighbor, 'waiting for neighbor: %s' % my_neighbor))"]
            cmd += ["rpc('localhost', 'gather_xstats', ())"]
            return cmd
        return ''

    def exposed_get_kill(self):
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
            beats = [beat[4] for beat in beats]
            #print '%s : Active clients: %s\r' % (stime(), beats)
            #stdscr.addstr(0, 0, '%s : Active clients: %s\r' % (stime(), beats))

            s = ''
            for key, value in STATSD.iteritems():
                s += '%s:\t %s\n' % (key,value)
            f = open(STATS_FILE, 'w')
            f.write(s)
            f.close()
            #print '%s : Writing out Stats' % stime()
            #stdscr.addstr(20, 0, '%s : Writing out Stats' % stime())

            #stdscr.refresh()
            #stdscr.clearok(1)
            
            time.sleep(CHECK_PERIOD)


class Runner(threading.Thread):
    def run(self):
        while True:
            try:
                [rpc('localhost', 'hard_restart', (backbone, )) for backbone in BACKBONES]
                rpc('localhost', 'new_exp', ())
            except Exception, e:
                print e
                time.sleep(1)
                pass
            else:
                break
                


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
        BACKBONE_TOPO[NAME_LOOKUP[line.split(':')[0].strip()]] = tuple([NAME_LOOKUP[l.strip()] for l in line.split(':')[1].split(',')])

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

    print 'Master_Server killing all clients'
    ts = []
    ts += [Thread(target=ssh_run, args=(host, STOP_CMD, None, False)) for host in CLIENTS]
    ts += [Thread(target=ssh_run, args=(host, KILL_LS, None, False)) for host in CLIENTS]
    ts += [Thread(target=ssh_run, args=(host, STOP_CMD, None, False)) for host in BACKBONES]
    ts += [Thread(target=ssh_run, args=(host, KILL_LS, None, False)) for host in BACKBONES]

    [t1.start() for t1 in ts]
    [t1.join() for t1 in ts]

    print 'Exiting, please wait...'
    FINISHED_EVENT.clear()
    printer.join()

    print 'Finished.'
    
    sys.exit(0)
