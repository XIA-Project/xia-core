#!/usr/bin/python

import rpyc, time, threading, sys, socket, thread, os, signal, copy
from threading import Thread
from rpyc.utils.server import ThreadPoolServer
from os.path import splitext
from plcommon import TimedThreadedDict, check_output, rpc, printtime, stime
from random import sample, choice, randint, seed, shuffle
from subprocess import Popen, PIPE
import logging
logging.basicConfig()

RPC_PORT = 43278;
CLIENT_PORT = 3000
CHECK_PERIOD = 3
STATS_TIMEOUT = 3
FOURID_MAX_PING_TIME = 15

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
CLIENT_TOPO = {} # hostname --> [hostname]
PAIRWISE_PING = {} # (hostname,hostname,...) --> (backbone_hostname,client_hostname)
FINISHED_EVENT = threading.Event()
MAX_EXPERIMENT_TIMEOUT = 150 # seconds
PLANETLAB_DIR = '/home/cmu_xia/fedora-bin/xia-core/experiments/planetlab/'
LOG_DIR = PLANETLAB_DIR + 'logs/'
STATS_FILE = LOG_DIR + 'stats.txt'
LATLON_FILE = PLANETLAB_DIR + 'IPLATLON'
NAMES_FILE = PLANETLAB_DIR + 'names'
BACKBONE_TOPO_FILE = PLANETLAB_DIR + 'backbone_topo'
FOURID_TOPO_FILE = PLANETLAB_DIR + '4ID_topo'
FOURID_G_INDEX = None
TYPE = None

# note that killing local server is not in this one
STOP_CMD = '"sudo killall sh; sudo killall init.sh; sudo killall rsync; sudo ~/fedora-bin/xia-core/bin/xianet stop; sudo killall mapper_client.py; sudo killall xping; sudo killall xtraceroute; sudo killall ping; sudo killall traceroute; sudo killall webserver.py; sudo killall proxy.py; sudo killall browser_mockup.py"'
KILL_LS = '"sudo killall -s INT local_server.py; sudo killall -s INT python; sleep 5; sudo killall local_server.py; sudo killall python"'
START_CMD = '"curl https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/init.sh > ./init.sh && chmod 755 ./init.sh && ./init.sh && python -u ~/fedora-bin/xia-core/experiments/planetlab/scripts/local_server.py"'
FOURID_SETUP_CMD = '"curl https://raw.github.com/XIA-Project/xia-core/develop/experiments/planetlab/init.sh > ./init.sh && chmod 755 ./init.sh && ./init.sh && python -u ~/fedora-bin/xia-core/experiments/planetlab/scripts/local_server.py setup"'
SSH_CMD = 'ssh -o StrictHostKeyChecking=no -o ConnectTimeout=5 cmu_xia@'
XIANET_FRONT_CMD = 'sudo ~/fedora-bin/xia-core/bin/xianet -v -r -P'
XIANET_FRONT_HOST_CMD = 'sudo ~/fedora-bin/xia-core/bin/xianet -v -t -P'
XIANET_FRONT_4ID_CMD = 'sudo ~/fedora-bin/xia-core/bin/xianet -v -4r -P'
XIANET_BACK_CMD = '-f eth0 start'
XIANET_BACK_4ID_CMD = '-I eth0 start'

WEBSERVER_CMD = '/home/cmu_xia/fedora-bin/xia-core/experiments/planetlab/webserver_mockup.sh'
WEBSERVER_SID = 'f0afa824a36f2a2d95c67ff60f61200e48006625'
PROXY_SID = '91afa824a36f2a2d95c67ff60f61200e48006678'
BLANK_FOURID = '4500000000010000fafa00000000000000000000'

NUMEXP = 1
NEW_EXP_TIMER = None
CLIENTS = [] # hostname
PRINT_VERB = [] # print verbosity
NODE_WATCHERS = {} # hostname -> [(NodeWatcher Thread, goOnEvent)]
NEW_EXP_LOCK = thread.allocate_lock()
NODE_WATCHERS_LOCK = thread.allocate_lock()
PAIRWISE_PING_LOCK = thread.allocate_lock()
SINGLE_EXPERIMENT = True
SAME_TEST_NODES = False
FOURID_EXPERIMENT = True
APP_EXPERIMENT = True

class NodeWatcher(threading.Thread):
    def preexec(self): # Don't forward signals.
        os.setpgrp()

    def __init__(self, host, goOnEvent, finishEvent, fourid_setup=False):
        super(NodeWatcher, self).__init__()
        self.goOnEvent = goOnEvent
        self.finishEvent = finishEvent
        self.host = host
        self.out = open('/tmp/%s-log' % (self.host),'w',0)
        self.fourid_setup = fourid_setup

    def __del__(self):
        self.out.close()

    def print_write(self, s):
        if self.host in PRINT_VERB: printtime('%s: %s' % (self.host, s))
        self.out.write('%s: %s\n' % (stime(), s))
        
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
        p = Popen(SSH_CMD+'%s %s' % (self.host, cmd), shell=True, stdout=PIPE, stderr=PIPE, preexec_fn = self.preexec)
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
        if checkRC and self.goOnEvent.isSet():
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
                if self.fourid_setup:
                    self.ssh_run(FOURID_SETUP_CMD, waitForCompletion=False)
                else:
                    self.ssh_run(START_CMD, waitForCompletion=False)
            except Exception, e:
                if self.finishEvent.isSet():
                    self.print_write('NW.run Exception: %s' % e)
                    try:
                        rpc('localhost', 'error', ('Startup', self.host))
                    except:
                        pass
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

    def exposed_get_clients(self):
        return CLIENTS

    def exposed_get_experiment_nodes(self):
        return self.exposed_get_backbone() + self.exposed_get_clients()

    def exposed_get_gateway_xhost(self, type):
        host = self.exposed_get_gateway_host(type,self._host)

        try: 
            xhost =  'RE AD:%s HID:%s' % \
                (rpc(host, 'get_ad', ()), rpc(host, 'get_hid', ()))
            return xhost
        except:
            self.exposed_error('get_xhost: %s' % type, host)
            

    def exposed_get_gateway_host(self, type, host):
        A = True if host in BACKBONES else False
        if type == '6RD':
            if A:
                return self.exposed_get_sixrd_A()
            else:
                return self.exposed_get_sixrd_B()
        elif type == '4ID':
            if A:
                return self.exposed_get_fourid_GA()
            else:
                return self.exposed_get_fourid_GB()
        elif type == 'SDN':
            if A:
                return self.exposed_get_sdn_A()
            else:
                return self.exposed_get_sdn_B()



    def exposed_get_fourid_CA(self):
        return BACKBONES[0]

    def exposed_get_fourid_CB(self):
        return CLIENTS[0]

    def exposed_get_sixrd_A(self):
        return BACKBONES[4]

    def exposed_get_sixrd_B(self):
        return CLIENTS[4]

    def exposed_get_fourid_GA(self):
        return BACKBONES[FOURID_G_INDEX]

    def exposed_get_fourid_GB(self):
        return CLIENTS[FOURID_G_INDEX]

    def orchestrate_pairwise_ping(self):
        PAIRWISE_PING_LOCK.acquire()
        cur_exp = tuple(CLIENTS)
        if cur_exp in PAIRWISE_PING:
            PAIRWISE_PING_LOCK.release()
            return PAIRWISE_PING[cur_exp]
        min_ping = 5000
        min_pair = None
        for node in BACKBONES[1:]:
            while True:
                try:
                    out = rpc(node, 'get_ping', (CLIENTS[1:],))[0]
                    if float(out[0]) == -1: continue
                    if float(out[0]) < min_ping:
                        min_ping = float(out[0])
                        min_pair = (node, out[1])
                    break; # try next node
                except Exception, e:
                    time.sleep(1)
        PAIRWISE_PING[cur_exp] = min_pair
        PAIRWISE_PING_LOCK.release()
        return min_pair

    def exposed_get_sdn_A(self):
        return self.orchestrate_pairwise_ping()[0]

    def exposed_get_sdn_B(self):
        return self.orchestrate_pairwise_ping()[1]

    def exposed_get_fourid_nodes(self):
        fouridnodes = [self.exposed_get_fourid_CA(), self.exposed_get_sixrd_A(),
                       self.exposed_get_fourid_GA(), self.exposed_get_sdn_A(),
                       self.exposed_get_fourid_CB(), self.exposed_get_sixrd_B(),
                       self.exposed_get_fourid_GB(), self.exposed_get_sdn_B()]
        return fouridnodes

    def exposed_get_fourid_neighbor_xhost(self, type, src):
        printtime("<<<< GET 4ID NEIGHBORS >>>>")
        if type == '6RD': gA = self.exposed_get_sixrd_A(); gB = self.exposed_get_sixrd_B()
        if type == '4ID': gA = self.exposed_get_fourid_GA(); gB = self.exposed_get_fourid_GB()
        if type == 'SDN': gA = self.exposed_get_sdn_A(); gB = self.exposed_get_sdn_B()

        print "GETTING FROM: %s" % self.exposed_get_fourid_CA()
        s = 'AD:%s HID:%s' % (rpc(self.exposed_get_fourid_CA(), 'get_ad', ()), 
                              rpc(self.exposed_get_fourid_CA(), 'get_hid', ()))

        print "GETTING FROM: %s" % self.exposed_get_fourid_CB()
        d = 'AD:%s HID:%s' % (rpc(self.exposed_get_fourid_CB(), 'get_ad', ()), 
                              rpc(self.exposed_get_fourid_CB(), 'get_hid', ()))

        if self._host in CLIENTS:
            src = not src

        if src: 
            print "GETTING FROM: %s" % gB
            ad = rpc(gB, 'get_ad', ())
            print "GETTING FROM: %s" % gA
            fourid = rpc(gA, 'get_fourid', ())
            return 'RE AD:%s IP:%s %s' % (ad, fourid, s)
                                          
        else:
            print "GETTING FROM: %s" % gA
            ad = rpc(gA, 'get_ad', ())
            print "GETTING FROM: %s" % gB
            fourid = rpc(gB, 'get_fourid', ())
            return 'RE AD:%s IP:%s %s' % (ad, fourid, d)
                                          
    
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

    def exposed_get_neighbor_webserver(self, type):
        printtime("<<<< GET WEBSERVER >>>>")
        if type == "tunneling" and self._host in CLIENTS:
            neighbor = [client for client in CLIENTS if client != self._host][0]
        elif type in ['6RD','4ID','SDN']:
            neighbor = BACKBONES[0]
        while True:
            try:
                ad = 'AD:%s' % rpc(neighbor, 'get_ad', ())
                hid = 'HID:%s' % rpc(neighbor, 'get_hid', ())
                if type == "tunneling":
                    fourid = 'IP:%s' % BLANK_FOURID
                    s = 'RE %s %s %s %s SID:%s' % \
                        (ad, fourid, ad, hid, WEBSERVER_SID)
                else:
                    gad = 'AD:%s' % rpc( \
                        self.exposed_get_gateway_host(type,self._host), 'get_ad', ())
                    fourid = 'IP:%s' % rpc( \
                        self.exposed_get_gateway_host(type,neighbor), 'get_fourid', ())
                    s = 'RE %s %s %s %s SID:%s' % \
                        (gad, fourid, ad, hid, WEBSERVER_SID)
                return s
            except:
                pass

    def exposed_get_proxy_address(self, type):
        printtime("<<<< GET PROXY ADDR >>>>")
        me = CLIENTS[0]
        neighbor = BACKBONES[0]
        while True:
            try:
                gad = 'AD:%s' % rpc( \
                    self.exposed_get_gateway_host(type,neighbor), 'get_ad', ())
                ad = 'AD:%s' % rpc(self._host, 'get_ad', ())
                hid = 'HID:%s' % rpc(self._host, 'get_hid', ())
                fourid = 'IP:%s' % rpc( \
                    self.exposed_get_gateway_host(type,self._host), 'get_fourid', ())
                s = 'RE %s %s %s %s SID:%s' % \
                    (gad, fourid, ad, hid, PROXY_SID)
                return s
            except:
                time.sleep(1)
                print "Trying to get proxy addres..."
                pass


    def exposed_get_neighbor_host(self):
        if self._host in CLIENTS:
            return [client for client in CLIENTS if client != self._host][0]
        elif self._host in BACKBONES:
            return CLIENTS[0]

    def exposed_check_done_type(self, type):
        cur_exp = tuple(CLIENTS)
        i = ['6RD', '4ID', 'SDN'].index(type)
        if len(STATSD[cur_exp][i]) == 6:
            return True
        return False

    def exposed_browser_stats(self, latency, type):
        print "<<<< Browser Stats >>>>"
        cur_exp = tuple(CLIENTS)

        if type == 'tunneling':
            STATSD[cur_exp]['browser'] = (self._host, CLIENTS[0], latency)

            if 'stats' in PRINT_VERB: printtime('%s:\t %s\n' % (cur_exp,STATSD[cur_exp]))
            self.exposed_new_exp()
        else:
            print type
            i = ['6RD','4ID','SDN'].index(type)
            STATSD[cur_exp][i]['browser'] = (self._host, CLIENTS[0], latency)
            if 'stats' in PRINT_VERB: printtime('%s:\t %s\n' % (cur_exp,STATSD[cur_exp]))            
            #l = STATSD[cur_exp]
            #if len(l[0]) == 6 and len(l[1]) == 6 and len(l[2]) == 6:
            if len(STATSD[cur_exp][i]) == 6:
                print "<<< GOT ALL STATS!! >>>"
                self.exposed_new_exp()

    def exposed_stats(self, backbone_name, ping, hops):        
        cur_exp = tuple(CLIENTS)
        m = 'A' if self._host == CLIENTS[0] else 'B'
        n = 'B' if self._host == CLIENTS[0] else 'A'
        STATSD[cur_exp]['backbone-%s' % m] = (self._host, backbone_name, ping, hops)

        # went to the same BB node -- we don't handle this
        if len(STATSD[cur_exp]) >= 2:
            my_bb = STATSD[cur_exp]['backbone-%s' % m][1]
            n_bb = STATSD[cur_exp]['backbone-%s' % n][1]
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
        m = 'A' if self._host == CLIENTS[0] else 'B'
        n = 'B' if self._host == CLIENTS[0] else 'A'
        neighbor = CLIENTS[1] if self._host == CLIENTS[0] else CLIENTS[0]
        STATSD[cur_exp]['%s%s' % (m,n)] = (self._host, neighbor, xping, xhops)

        if 'xstats' in PRINT_VERB: printtime('%s:\t %s\n' % (cur_exp,STATSD[cur_exp]))

    def exposed_fidstats(self, type, neighbor, ping, hops):
        cur_exp = tuple(CLIENTS)

        #(DC, NYC, UMassD, UNC, MIT) --> 
        #  [['6RD', ('6RD-AB', 'KC', 'DC', '40.000', 4),
        #           ('6RD-GG' Houston', 'NYC', '40.243', 14), 
        #           ('6RD-BA' 'DC', 'KC', '40.000', 4),
        #           ('6RD-AG', 'KC', 'Houston', '30.000', 1), 
        #           ('6RD-BG', 'DC', 'NYC', '10.000', 1)], 
        #   ['4ID', ...], ['SDN',...]]

        i = ['6RD','4ID','SDN'].index(type.split('-')[0])
        STATSD[cur_exp][i][type] = (self._host, neighbor, ping, hops)

        if 'stats' in PRINT_VERB: printtime('%s:\t %s\n' % (cur_exp,STATSD[cur_exp]))

    def init_statsd(self, new_clients):
        cur_exp = tuple(new_clients)
        if FOURID_EXPERIMENT:
            STATSD[cur_exp] = [{},{},{}] # 6RD, 4ID, SDN
        else:
            STATSD[cur_exp] = {}

    def build_topo(self, new_clients):
        # no node can have more than three edges
        # must be connected
        # 7 edges

        # A --> [B, C, D, E]
        # (0,1) (0,2) (0,3) (0,4)


        nc = copy.deepcopy(new_clients)

        for c in nc:
            CLIENT_TOPO[c] = []
        
        links = [(0,1),(0,2),(0,3),(0,4)]
        for (a,b) in links:
            CLIENT_TOPO[nc[a]].append(nc[b])
            CLIENT_TOPO[nc[b]].append(nc[a])

    def exposed_new_exp(self, host=None):
        global CLIENTS, NUMEXP, NEW_EXP_TIMER, PRINT_VERB, NEW_EXP_LOCK, FOURID_G_INDEX, TYPE

        if SINGLE_EXPERIMENT and NUMEXP == 2:
            return

        if NEW_EXP_LOCK.locked():
            return
        NEW_EXP_LOCK.acquire()
        print "<<< lock aquired >>>"

        # some host errored that's not currently in the experiment
        if host and host not in CLIENTS and host not in BACKBONES:
            NEW_EXP_LOCK.release()
            print "<<< nope: lock released >>>"
            return

        try:
            cur_exp = tuple(CLIENTS)
            if len(STATSD[cur_exp]) != 4 and not FOURID_EXPERIMENT:
                printtime("<<<< TIMEOUT!! (%s) >>>>" % cur_exp)
            if len(STATSD[cur_exp]) != 3 and FOURID_EXPERIMENT:
                printtime("<<<< TIMEOUT!! (%s) >>>>" % cur_exp)
        except:
            pass

        # pick new test clients
        while True:
            if not FOURID_EXPERIMENT:
                while True:
                    new_clients = sample(NAMES, 2)
                    new_clients = [NAME_LOOKUP[client] for client in new_clients]
                    if new_clients[0] not in CLIENTS and new_clients[1] not in CLIENTS:
                        break
                if SAME_TEST_NODES:
                    new_clients = [NAME_LOOKUP['planetlab1.tsuniv.edu'],NAME_LOOKUP['planetlab5.cs.cornell.edu']]
                new_clients = sorted(new_clients)
            else:
                while True:
                    while True:
                        new_clients = [NAME_LOOKUP[choice(NAMES)]]
                        if new_clients[0] not in CLIENTS:
                            break
                    if SAME_TEST_NODES:
                        new_clients = [NAME_LOOKUP['DC']]
                    print new_clients[0]
                    IP_LOOKUP[socket.gethostbyname(new_clients[0])] = new_clients[0]
                    self.exposed_hard_restart(new_clients[0], setup=True)
                    i = 0
                    while new_clients[0] in NODE_WATCHERS and i < 30:
                        try:
                            rpc(new_clients[0],'get_hello',())
                            break
                        except:
                            time.sleep(1)
                            i += 1
                    if new_clients[0] not in NODE_WATCHERS or i >= 30: # client crashed
                        continue
                        
                    i = 0
                    while i < 5: # only try five times since some nodes have ping issue and we don't want to lock
                        try:
                            print "Trying Ping"
                            test_clients = [NAME_LOOKUP[c] for c in sample(NAMES, 50)]
                            if new_clients[0] in test_clients: continue
                            pings = rpc(new_clients[0], 'get_ping', (test_clients, ))
                            print pings
                            if float(pings[3][0]) < FOURID_MAX_PING_TIME:
                                break
                        except Exception, e:
                            print "Ping didn't work"
                            print e
                            #print pings
                            time.sleep(1)
                        i += 1
                    if i < 5: # if we successfully got our topo
                        break
                    else: # take down client
                        print 'need to try new ping client'
                        NODE_WATCHERS[new_clients[0]].clearGoOn()
                        while new_clients[0] in NODE_WATCHERS: print "waiting for node to go down"; time.sleep(1)
                print "Done Ping"
                ping_hosts = [p[1] for p in pings]
                new_clients += ping_hosts[:4]
                if SAME_TEST_NODES:
                    new_clients = [new_clients[0]]+['NYC','Atlanta','Cleveland', 'Houston']
                    print new_clients
                    new_clients = [new_clients[0]]+[NAME_LOOKUP[c] for c in new_clients[1:]]
                print "building topo"
                self.build_topo(new_clients)
                print "done topo"
                NODE_WATCHERS[new_clients[0]].clearGoOn()
                while new_clients[0] in NODE_WATCHERS: print "waiting for node to go down"; time.sleep(1)

            # Make sure we haven't done this experiment before
            if tuple(new_clients) not in STATSD:
                self.init_statsd(new_clients)
                break

        CLIENTS = new_clients
        for client in CLIENTS:
            while True:
                try:
                    IP_LOOKUP[socket.gethostbyname(client)] = client
                    break
                except:
                    print "Error doing Client lookup in new exp: %s" % client
                    time.sleep(1)
        
        [PRINT_VERB.append(c) for c in CLIENTS]

        printtime('<<<< new experiment (%s): %s >>>>' % (NUMEXP, CLIENTS))
        FOURID_G_INDEX = randint(1,4)
        printtime("<<<<<<<<RANDINT: %s>>>>>>>>" % FOURID_G_INDEX)
        TYPE = choice(['6RD','4ID','SDN'])


        for host in NODE_WATCHERS:
            self.exposed_hard_restart(host)

        for host in CLIENTS:
            self.exposed_hard_restart(host)

        printtime('<<< FINISHED RELAUNCHING HOSTS >>>')


        NUMEXP += 1
        if NEW_EXP_TIMER: NEW_EXP_TIMER.cancel()
        NEW_EXP_TIMER = threading.Timer(MAX_EXPERIMENT_TIMEOUT, self.exposed_new_exp)
        NEW_EXP_TIMER.start()
        NEW_EXP_LOCK.release()

        printtime('<<< LOCK RELEASED >>>')

    def exposed_error(self, msg, host):
        printtime('<<<< %s  (error!): %s >>>>' % (host, msg))
        if SINGLE_EXPERIMENT:
            return
        host = self._host if host == None else host
        if host not in BACKBONES:
            printtime('<<<< Remvoing bad host: %s from NAMES >>>>' % host)
            NAMES.remove(HOSTNAME_LOOKUP[host]) # remove this misbehaving host from further experiments
        self.exposed_new_exp(host=host)
            
    def exposed_hard_restart(self, host, setup=False):
        NODE_WATCHERS_LOCK.acquire()
        if host in NODE_WATCHERS:
            NODE_WATCHERS[host].clearGoOn()
        else:
            goEv = threading.Event()
            goEv.set()
            finishEv = threading.Event()
            finishEv.set()
            nw = NodeWatcher(host=host, goOnEvent=goEv, finishEvent=finishEv, fourid_setup=setup)
            nw.start()
            NODE_WATCHERS[host] = nw
        NODE_WATCHERS_LOCK.release()

    def exposed_get_xianet(self, neighbor_host = None, host = None):
        host = self._host if host == None else host
        if host in CLIENTS:
            if FOURID_EXPERIMENT:
                links = list(set([tuple(sorted((client,neighbor))) for client in CLIENTS for neighbor in CLIENT_TOPO[client]]))
                neighbors = ['%s:5120%s' % ([socket.gethostbyname(n) for n in link if n != host][0],links.index(link)) for link in links if host in link]
                if host == self.exposed_get_fourid_CB():
                    return "Popen('%s %s %s', shell=True)" % (XIANET_FRONT_CMD, ','.join(neighbors[0:4]), XIANET_BACK_CMD)
                else:
                    return "Popen('%s %s %s', shell=True)" % (XIANET_FRONT_4ID_CMD, ','.join(neighbors[0:3]), XIANET_BACK_4ID_CMD)
            else:
                return "Popen('%s %s:%s %s', shell=True)" % (XIANET_FRONT_CMD, socket.gethostbyname(neighbor_host), CLIENT_PORT, XIANET_BACK_CMD)
        elif host in BACKBONES:
            links = list(set([tuple(sorted((backbone,neighbor))) for backbone in BACKBONES for neighbor in BACKBONE_TOPO[backbone]]))
            neighbors = ['%s:500%s' % ([socket.gethostbyname(n) for n in link if n != host][0],links.index(link)) for link in links if host in link]
            if FOURID_EXPERIMENT:
                if host == self.exposed_get_fourid_CA():
                    return "Popen('%s %s %s', shell=True)" % (XIANET_FRONT_CMD, ','.join(neighbors[0:4]), XIANET_BACK_CMD)
                else:
                    return "Popen('%s %s %s', shell=True)" % (XIANET_FRONT_4ID_CMD, ','.join(neighbors[0:3]), XIANET_BACK_4ID_CMD)
            else:
                if neighbor_host:
                    neighbors.append('%s:%s' % (socket.gethostbyname(neighbor_host), CLIENT_PORT))
                return "Popen('%s %s %s', shell=True)" % (XIANET_FRONT_CMD, ','.join(neighbors[0:4]), XIANET_BACK_CMD)
        return ''

    def exposed_get_commands(self, host = None):
        host = self._host if host == None else host
        if 'master' in PRINT_VERB: printtime('MASTER: %s checked in for commands' % host)

        if FOURID_EXPERIMENT:
            print TYPE
            cmd = [self.exposed_get_xianet(host=host)]
            if CLIENTS:
                cmd += ["self.exposed_wait_for_neighbors(rpc(MASTER_SERVER, 'get_experiment_nodes', ()), 'waiting for xianet to start on all nodes')"]
                cmd += ["self.exposed_gather_fourid_stats('%s')" % TYPE]
            return cmd

        if host in BACKBONES:
            return [self.exposed_get_xianet(host = host)]
        elif host in CLIENTS:
            if not NEW_EXP_LOCK.locked():
                cmd = ["my_backbone = self.exposed_gather_stats()[1]"]
                cmd += ["self.exposed_wait_for_neighbors([my_backbone], 'waiting for backbone: %s' % my_backbone)"]
                cmd += ["rpc(my_backbone, 'soft_restart', (my_name, ))"]
                cmd += ["xianetcmd = rpc(MASTER_SERVER, 'get_xianet', (my_backbone, ))"]
                cmd += ["printtime(xianetcmd)"]
                cmd += ["exec(xianetcmd)"]
                cmd += ["self.exposed_wait_for_neighbors(['localhost'], 'waiting for xianet to start')"]
                cmd += ["my_neighbor = rpc(MASTER_SERVER, 'get_neighbor_host', ())"]
                cmd += ["printtime(my_neighbor)"]
                cmd += ["self.exposed_wait_for_neighbors([my_backbone], 'waiting for backbone: %s' % my_backbone)"]
                cmd += ["self.exposed_wait_for_neighbors([my_neighbor], 'waiting for neighbor: %s' % my_neighbor)"]
                cmd += ["self.exposed_gather_xstats()"]
                if APP_EXPERIMENT:
                    if host == CLIENTS[0]:
                        cmd += ["check_output('%s')" % WEBSERVER_CMD]
                    else:
                        cmd += ["self.exposed_gather_browser_stats('tunneling')"]
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
    seed()

    latlonfile = open(LATLON_FILE, 'r').read().split('\n')
    for ll in latlonfile:
        ll = ll.split(' ')
        LATLOND[ll[0]] = ll[1:-1]

    ns = open(NAMES_FILE,'r').read().split('\n')[:-1]
    NAMES = [n.split('#')[1].strip() for n in ns]
    nl = [line.split('#') for line in ns]
    NAME_LOOKUP = dict((n.strip(), host.strip()) for (host, n) in nl)
    HOSTNAME_LOOKUP = dict((host.strip(), n.strip()) for (host, n) in nl)

    if FOURID_EXPERIMENT:
        topo_file = FOURID_TOPO_FILE
    else:
        topo_file = BACKBONE_TOPO_FILE
        MAX_EXPERIMENT_TIMEOUT = 180

    lines = open(topo_file,'r').read().split('\n')
    for line in lines:
        BACKBONES.append(NAME_LOOKUP[line.split(':')[0]])
        BACKBONE_TOPO[NAME_LOOKUP[line.split(':')[0].strip()]] = tuple([NAME_LOOKUP[l.strip()] for l in line.split(':')[1].split(',')])
    for backbone in BACKBONES:
        IP_LOOKUP[socket.gethostbyname(backbone)] = backbone
        NAMES.remove(HOSTNAME_LOOKUP[backbone])

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
