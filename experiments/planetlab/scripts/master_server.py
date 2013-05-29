#!/usr/bin/python

import rpyc, time, threading, sys, socket, thread, os, signal
from threading import Thread
from rpyc.utils.server import ThreadPoolServer
from os.path import splitext
from plcommon import TimedThreadedDict, check_output, rpc, printtime, stime
from random import choice
from subprocess import Popen, PIPE
import logging
logging.basicConfig()

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
HOSTNAME_LOOKUP = {} # hostname --> names
IP_LOOKUP = {} # IP --> hostname
BACKBONES = [] # hostname
BACKBONE_TOPO = {} # hostname --> [hostname]
FINISHED_EVENT = threading.Event()
MAX_EXPERIMENT_TIMEOUT = 300 # seconds
PLANETLAB_DIR = '/home/cmu_xia/fedora-bin/xia-core/experiments/planetlab/'
LOG_DIR = PLANETLAB_DIR + 'logs/'
STATS_FILE = LOG_DIR + 'stats-tunneling.txt'
LATLON_FILE = PLANETLAB_DIR + 'IPLATLON'
NAMES_FILE = PLANETLAB_DIR + 'names'
BACKBONE_TOPO_FILE = PLANETLAB_DIR + 'backbone_topo'

# note that killing local server is not in this one
STOP_CMD = '"sudo killall sh; sudo killall init.sh; sudo killall rsync; sudo ~/fedora-bin/xia-core/bin/xianet stop; sudo killall mapper_client.py; sudo killall xping; sudo killall xtraceroute; sudo killall ping; sudo killall traceroute"'
KILL_LS = '"sudo killall -s INT local_server.py; sudo killall -s INT python; sleep 5; sudo killall local_server.py; sudo killall python"'
START_CMD = '"curl https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/init.sh > ./init.sh && chmod 755 ./init.sh && ./init.sh"'
SSH_CMD = 'ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 cmu_xia@'
XIANET_FRONT_CMD = 'sudo ~/fedora-bin/xia-core/bin/xianet -v -r -P'
XIANET_FRONT_HOST_CMD = 'sudo ~/fedora-bin/xia-core/bin/xianet -v -t -P'
XIANET_BACK_CMD = '-f eth0 start'

NUMEXP = 1
NEW_EXP_TIMER = None
CLIENTS = [] # hostname
PRINT_VERB = [] # print verbosity
NODE_WATCHERS = {} # hostname -> [(NodeWatcher Thread, goOnEvent)]
CHECKED_IN_NODES = []
NEW_EXP_LOCK = thread.allocate_lock()
NODE_WATCHERS_LOCK = thread.allocate_lock()
SINGLE_EXPERIMENT = False
SAME_TEST_NODES = False

class NodeWatcher(threading.Thread):
    def __init__(self, host, goOnEvent, finishEvent):
        super(NodeWatcher, self).__init__()
        self.goOnEvent = goOnEvent
        self.finishEvent = finishEvent
        self.host = host
        self.out = open('/tmp/%s-log' % (self.host),'w',0)

    def __del__(self):
        self.out.close()

    def print_write(self, s):
        if self.host in PRINT_VERB: printtime('%s: %s' % (self.host, s))
        self.out.write('%s: %s' % (stime(), s))
        
    def std_listen(self, handle, out):
        while True:
            line = handle.readline()
            if not line:
                return
            if out: out.write('%s: %s' %(stime(), line))

    def hard_stop(self):
        self.ssh_run(STOP_CMD, checkRC=False)
        self.ssh_run(KILL_LS, checkRC=False)

    def ssh_run(self, cmd, checkRC=True, waitForCompletion=True):
        def target(p):
            p.wait()
            [t.join() for t in ts]

        self.print_write('launching subprocess: %s' % cmd)
        p = Popen(SSH_CMD+'%s %s' % (self.host, cmd), shell=True, stdout=PIPE, stderr=PIPE)
        ts = [Thread(target=self.std_listen, args=(p.stdout, self.out))]
        ts += [Thread(target=self.std_listen, args=(p.stderr, self.out))]
        [t.start() for t in ts]
            
        t = Thread(target=target, args=(p, ))
        t.start()
        while waitForCompletion or self.goOnEvent.isSet():
            t.join(1)
            if not t.isAlive(): break
        if t.isAlive():
            os.kill(p.pid, signal.SIGTERM)
        self.print_write('finished running subprocess: %s' % cmd)
        if checkRC is True and self.goOnEvent.isSet():
            rc = p.returncode
            if rc is not 0:
                c = SSH_CMD+'%s %s' % (self.host, cmd)
                raise Exception("subprocess.CalledProcessError: Command '%s'" \
                                    "returned non-zero exit status %s" % (c, rc))

    def clearFinish(self):
        self.finishEvent.clear()

    def clearGoOn(self):
        self.goOnEvent.clear()

    def run(self):
        should_run = True
        while should_run:
            should_run = False
            self.print_write('launching...')
            self.hard_stop()
            try:
                self.ssh_run(START_CMD, waitForCompletion=False)
            except Exception, e:
                if self.finishEvent.isSet():
                    self.print_write('NW.run Exception: %s' % e)
                    try:
                        rpc('localhost', 'error', ('Startup', self.host))
                    except:
                        time.sleep(1)
            if self.finishEvent.isSet() and self.host in BACKBONES:
                should_run = True
                self.goOnEvent.set()
        self.hard_stop()
        self.print_write('finished running process')
        NODE_WATCHERS.pop(self.host)


class MasterService(rpyc.Service):
    def on_connect(self):        
        self._host = IP_LOOKUP[self._conn._config['endpoints'][1][0]]
        self._conn._config['allow_pickle'] = True

    def on_disconnect(self):
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
        printtime("<<<< GET NEIGHBORS >>>>")
        if self._host in CLIENTS:
            neighbor = [client for client in CLIENTS if client != self._host][0]
            while True:
                try:
                    return 'RE AD:%s HID:%s' % \
                        (rpc(neighbor, 'get_ad', ()), rpc(neighbor, 'get_hid', ()))
                except:
                    pass
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

        # went to the same BB node -- we don't handle this
        if len(STATSD[cur_exp]) >= 2:
            neighbor = [client for client in CLIENTS if client != self._host][0]
            bbs = [b[0] for b in STATSD[cur_exp][0:2]]
            my_bb = bbs[0][1] if self._host in bbs[0] else bbs[1][1]
            n_bb = bbs[0][1] if neighbor in bbs[0] else bbs[1][1]
            if my_bb == n_bb:
                printtime("<<<< Experiment went to the same backbone node >>>>")
                self.exposed_new_exp()
                pass

        if ping == '-1.000' or hops == -1:
            printtime("<<<< NODE can't see backbone >>>>")
            self.exposed_new_exp()
            pass

        if 'stats' in PRINT_VERB: printtime('%s:\t %s\n' % (cur_exp,STATSD[cur_exp]))

    def exposed_xstats(self, xping, xhops):
        cur_exp = tuple(CLIENTS)
        STATSD[cur_exp].append((cur_exp,'test',xping,xhops))

        if 'xstats' in PRINT_VERB: printtime('%s:\t %s\n' % (cur_exp,STATSD[cur_exp]))

        if len(STATSD[cur_exp]) is 4:
            self.exposed_new_exp()
            pass

    def exposed_new_exp(self, host=None):
        global CLIENTS, NUMEXP, NEW_EXP_TIMER, PRINT_VERB, NEW_EXP_LOCK

        if SINGLE_EXPERIMENT and NUMEXP == 2:
            return

        if NEW_EXP_LOCK.locked():
            return
        NEW_EXP_LOCK.acquire()

        # some host errored that's not currently in the experiment
        if host and host not in CLIENTS and host not in BACKBONES:
            return

        try:
            cur_exp = tuple(CLIENTS)
            if len(STATSD[cur_exp]) != 4:
                printtime("<<<< TIMEOUT!! (%s) >>>>" % cur_exp)
        except:
            pass

        while True:
            while True:
                client_a = choice(NAMES)
                if client_a not in CLIENTS: break

            while True:
                client_b = choice(NAMES)
                if client_b != client_a and client_b not in CLIENTS: break

            if SAME_TEST_NODES:
                client_a = 'planetlab1.tsuniv.edu'
                client_b = 'planetlab5.cs.cornell.edu'

            c = sorted([NAME_LOOKUP[client_a], NAME_LOOKUP[client_b]])
            if tuple(c) not in STATSD:
                break

        CLIENTS = c
        for client in CLIENTS:
            IP_LOOKUP[socket.gethostbyname(client)] = client
        
        [PRINT_VERB.append(c) for c in CLIENTS]

        printtime('<<<< new experiment (%s): %s >>>>' % (NUMEXP, CLIENTS))

        for host in NODE_WATCHERS:
            self.exposed_hard_restart(host)

        for client in CLIENTS:
            self.exposed_hard_restart(client)

        NUMEXP += 1
        if NEW_EXP_TIMER: NEW_EXP_TIMER.cancel()
        NEW_EXP_TIMER = threading.Timer(MAX_EXPERIMENT_TIMEOUT, self.exposed_new_exp)
        NEW_EXP_TIMER.start()
        NEW_EXP_LOCK.release()

    def exposed_error(self, msg, host):
        printtime('<<<< %s  (error!): %s >>>>' % (host, msg))
        if SINGLE_EXPERIMENT:
            return
        host = self._host if host == None else host
        if host not in BACKBONES:
            printtime('<<<< Remvoing bad host: %s from NAMES >>>>' % host)
            NAMES.remove(HOSTNAME_LOOKUP[host]) # remove this misbehaving host from further experiments
        self.exposed_new_exp(host=host)
            
    def exposed_hard_restart(self, host):
        NODE_WATCHERS_LOCK.acquire()
        if host in NODE_WATCHERS:
            NODE_WATCHERS[host].clearGoOn()
        else:
            goEv = threading.Event()
            goEv.set()
            finishEv = threading.Event()
            finishEv.set()
            nw = NodeWatcher(host=host, goOnEvent=goEv, finishEvent=finishEv)
            nw.start()
            NODE_WATCHERS[host] = nw
        NODE_WATCHERS_LOCK.release()

    def exposed_get_xianet(self, neighbor_host = None, host = None):
        host = self._host if host == None else host
        if host in CLIENTS:
            return "Popen('%s %s:%s %s', shell=True)" % (XIANET_FRONT_CMD, socket.gethostbyname(neighbor_host), CLIENT_PORT, XIANET_BACK_CMD)
        elif host in BACKBONES:
            links = list(set([tuple(sorted((backbone,neighbor))) for backbone in BACKBONES for neighbor in BACKBONE_TOPO[backbone]]))
            neighbors = ['%s:500%s' % ([socket.gethostbyname(n) for n in link if n != host][0],links.index(link)) for link in links if host in link]
            if neighbor_host:
                neighbors.append('%s:%s' % (socket.gethostbyname(neighbor_host), CLIENT_PORT))
            return "Popen('%s %s %s', shell=True)" % (XIANET_FRONT_CMD, ','.join(neighbors[0:4]), XIANET_BACK_CMD)
        return ''

    def exposed_get_commands(self, host = None):
        host = self._host if host == None else host
        if 'master' in PRINT_VERB: printtime('MASTER: %s checked in for commands' % host)
        CHECKED_IN_NODES.append(host)
        if host in BACKBONES:
            return [self.exposed_get_xianet(host = host)]
        elif host in CLIENTS:
            if not NEW_EXP_LOCK.locked():
                cmd = ["my_backbone = self.exposed_gather_stats()[1]"]
                cmd += ["self.exposed_wait_for_neighbor(my_backbone, 'waiting for backbone: %s' % my_backbone)"]
                cmd += ["rpc(my_backbone, 'soft_restart', (my_name, ))"]
                cmd += ["xianetcmd = rpc(MASTER_SERVER, 'get_xianet', (my_backbone, ))"]
                cmd += ["printtime(xianetcmd)"]
                cmd += ["exec(xianetcmd)"]
                cmd += ["self.exposed_wait_for_neighbor('localhost', 'waiting for xianet to start')"]
                cmd += ["my_neighbor = rpc(MASTER_SERVER, 'get_neighbor_host', ())"]
                cmd += ["printtime(my_neighbor)"]
                cmd += ["self.exposed_wait_for_neighbor(my_backbone, 'waiting for backbone: %s' % my_backbone)"]
                cmd += ["self.exposed_wait_for_neighbor(my_neighbor, 'waiting for neighbor: %s' % my_neighbor)"]
                cmd += ["time.sleep(10)"]
                cmd += ["self.exposed_gather_xstats()"]
                return cmd

        
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
        html = '<html>\n<head>\n<title>Current Nodes In Topology</title>\n<meta http-equiv="refresh" content="60">\n</head>\n<body>\n<img src="%s">\n</body>\n</html>' % url
        return html

    def run(self):
        while self.goOnEvent.isSet():
            beats = HEARTBEATS.getClients()
            f = open('/var/www/html/map.html', 'w')
            f.write(self.buildMap(beats))
            f.close()
            beats = [beat[4] for beat in beats]

            s = ''
            for key, value in STATSD.iteritems():
                s += '%s:\t %s\n' % (key,value)
            f = open(STATS_FILE, 'w')
            f.write(s)
            f.close()
            
            time.sleep(CHECK_PERIOD)


class Runner(threading.Thread):
    def run(self):
        for backbone in BACKBONES:
            while True:
                try:
                    rpc('localhost', 'hard_restart', (backbone, ))
                    break;
                except Exception, e:
                    printtime('%s' % e)
                    time.sleep(1)                
        while True:
            try:
                rpc('localhost', 'new_exp', ())
                break
            except Exception, e:
                printtime('%s' % e)
                time.sleep(1)


if __name__ == '__main__':
    latlonfile = open(LATLON_FILE, 'r').read().split('\n')
    for ll in latlonfile:
        ll = ll.split(' ')
        LATLOND[ll[0]] = ll[1:-1]

    ns = open(NAMES_FILE,'r').read().split('\n')[:-1]
    NAMES = [n.split('#')[1].strip() for n in ns]
    nl = [line.split('#') for line in ns]
    NAME_LOOKUP = dict((n.strip(), host.strip()) for (host, n) in nl)
    HOSTNAME_LOOKUP = dict((host.strip(), n.strip()) for (host, n) in nl)

    BACKBONES = [NAME_LOOKUP[n.strip()] for n in NAMES[:11]]
    for backbone in BACKBONES:
        NAMES.remove(HOSTNAME_LOOKUP[backbone])
    lines = open(BACKBONE_TOPO_FILE,'r').read().split('\n')
    for line in lines:
        BACKBONE_TOPO[NAME_LOOKUP[line.split(':')[0].strip()]] = tuple([NAME_LOOKUP[l.strip()] for l in line.split(':')[1].split(',')])
    for backbone in BACKBONES:
        IP_LOOKUP[socket.gethostbyname(backbone)] = backbone
    IP_LOOKUP['127.0.0.1'] = socket.gethostbyaddr('127.0.0.1')

    PRINT_VERB.append('stats')
    PRINT_VERB.append('xstats')
    PRINT_VERB.append('master')
    [PRINT_VERB.append(b) for b in BACKBONES]

    printtime(('Threaded heartbeat server listening on port %d\n' 
              'press Ctrl-C to stop\n') % RPC_PORT)

    FINISHED_EVENT.set()
    printer = Printer(goOnEvent = FINISHED_EVENT)
    printer.start()

    runner = Runner()
    runner.start()

    try:
        t = ThreadPoolServer(MasterService, port = RPC_PORT)
        t.start()
    except Exception, e:
        printtime('%s' % e)

    FINISHED_EVENT.clear()

    printtime('Master_Server killing all clients')
    for host in NODE_WATCHERS:
        NODE_WATCHERS[host].clearFinish()
        NODE_WATCHERS[host].clearGoOn()
    while len(NODE_WATCHERS): time.sleep(1)

    printtime('Exiting, please wait...')
    printer.join()

    printtime('Finished.')
    
    sys.exit(0)
