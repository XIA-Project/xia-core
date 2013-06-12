#!/usr/bin/python

import rpyc, re, time, threading, thread, sys
from plcommon import check_output, rpc, printtime, check_both
from subprocess import call, Popen, PIPE
from rpyc.utils.server import ThreadPoolServer

RPC_PORT = 43278
KILL_XIANET = 'sudo killall xianet; sudo ~/fedora-bin/xia-core/bin/xianet stop'
PROXY_CMD = '/home/cmu_xia//fedora-bin/xia-core/experiments/planetlab/proxy_mockup.sh'
BROWSER_CMD = '/home/cmu_xia/fedora-bin/xia-core/experiments/planetlab/browser_mockup.py'
BROWSER_ADDR = 'www_s.xiaweb.com.xia'
ETC_HOSTS = '~/fedora-bin/xia-core/etc/hosts.xia'
XROUTE = '/home/cmu_xia/fedora-bin/xia-core/bin/xroute -v'
MASTER_SERVER = 'GS11698.SP.CS.CMU.EDU'
BEAT_PERIOD = 3
BROADCAST_HID = 'ffffffffffffffffffffffffffffffffffffffff'
PING_INTERVAL = .10
XPING_INTERVAL = .10
PING_COUNT = 50
XPING_COUNT = 50
WEBSERVER_CMD = '/home/cmu_xia/fedora-bin/xia-core/experiments/planetlab/webserver_mockup.sh'

myHID = ''
my_name = check_output("hostname")[0].strip()

FINISH_EVENT = threading.Event()    

FOURID_SETUP = False

def multi_ping(neighbors, np=PING_COUNT):
    print "<<<< Multi Ping >>>>"
    pingcmd = 'sudo ping -W 1 -i %s -c' % PING_INTERVAL
    
    processes = [(Popen('%s %s %s' % (pingcmd, 1, neighbor), shell=True, stdout=PIPE, stderr=PIPE), neighbor) for neighbor in neighbors]
    outs = [process[0].communicate() for process in processes]

    processes = [(Popen('%s %s %s' % (pingcmd, np, neighbor), shell=True, stdout=PIPE, stderr=PIPE), neighbor) for neighbor in neighbors]
    outs = [process[0].communicate() for process in processes]
    rcs = [process[0].wait() for process in processes]
    outs = zip(outs, rcs, [p[1] for p in processes])

    stats = []
    for out in outs:
        p = 5000
        try:
            l = sorted([float(x.split('time=')[1].split(' ')[0]) for x in out[0][0].split('\n')[1:-5]])[:5]
            print l
            p = float(sum(l))/len(l)
        except:
            pass
        stats.append((p,out[2]))
    stats = sorted(stats)
    stats = [(-1,stat[1]) if stat[0] == 5000 else stat for stat in stats]
    stats = [("%.3f" % stat[0], stat[1]) for stat in stats]
    return stats

def traceroute(neighbor, gateway=False):
    out = check_output('sudo traceroute -I -w 1 %s' % neighbor)
    print out
    if gateway:
        out = out[0].split('\n')
        z = my_name.split('.')[-2]
        stat = -1
        for o in out:
            stat = int(o.strip().split(' ')[0]) if z in o else stat
    else:
        stat = int(out[0].split("\n")[-2].strip().split(' ')[0])
        stat = -1 if stat is 30 else stat
    return stat

def xping(neighbor,tryUntilSuccess=False,src=None):
    xpingcmd = '/home/cmu_xia/fedora-bin/xia-core/bin/xping'
    if src:
        xpingcmd = '%s -s "%s"' % (xpingcmd, src)
    while True:
        s = '%s -t 5 "%s"' % (xpingcmd, neighbor)
        while True:
            try:
                printtime(s)
                out = check_output(s)
                printtime(out)
                stat = "%.3f" % float(out[0].split("\n")[-2].split('=')[1].split('/')[1])
                break
            except:
                pass
        s = '%s -t 30 -i %s -c %s "%s"' % (xpingcmd, XPING_INTERVAL, XPING_COUNT, neighbor)
        while True:
            try:
                printtime(s)
                out = check_output(s)
                break
            except:
                if not tryUntilSuccess:
                    break
        printtime(out)
        try:
            l = sorted([float(x.split('time=')[1].split(' ')[0]) for x in out[0].split('\n') if 'time=' in x])[:5]
            print l
            stat = sum(l) / len(l)
            #stat = "%.3f" % float(out[0].split("\n")[-2].split('=')[1].split('/')[1])
        except:
            stat = -1
        stat = "%.3f" % stat
        stat = -1 if stat == '=' else stat
        return stat

def xtraceroute(neighbor,tryUntilSuccess=True, src=None):
    cmd = '/home/cmu_xia/fedora-bin/xia-core/bin/xtraceroute -t 30'
    if src:
        cmd = '%s -s "%s"' % (cmd, src)
    cmd = '%s "%s"' % (cmd, neighbor)
    while tryUntilSuccess:
        try:
            printtime(cmd)
            out = check_output(cmd)
            printtime(out)
            break
        except Exception, e:
            printtime('%s' % e)
            pass
    stat = int(out[0].split('\n')[-2].split('=')[1].strip())
    stat = -1 if stat is 30 else stat
    printtime('%s' % stat)
    return stat
    
class MyService(rpyc.Service):
    def on_connect(self):
        self._conn._config['allow_pickle'] = True

    def on_disconnect(self):
        pass

    def exposed_get_hello(self):
        return 'hello'

    def exposed_gather_browser_stats(self, type):
        # write to etc/hosts.xia
        check_output('echo "%s %s" > %s' % (BROWSER_ADDR, rpc(MASTER_SERVER, 'get_neighbor_webserver', (type,)), ETC_HOSTS))

        print check_output('cat %s' % ETC_HOSTS)

        while True:
            try:
                check_output('ps -e | grep proxy.py')
                break
            except:
                if type == 'tunneling':
                    Popen(PROXY_CMD, shell=True)
                else:
                    proxy_dag = rpc(MASTER_SERVER, 'exposed_get_proxy_address', (type,))
                    print 'proxy going to start'
                    print proxy_dag
                    Popen('%s "%s"' % (PROXY_CMD, proxy_dag), shell=True)
                time.sleep(1)
        time.sleep(3)
        print 'running browser cmd'
        p = Popen(BROWSER_CMD,shell=True)
        p.wait()
        time.sleep(3)
        while True:
            (out, rc) = check_both(BROWSER_CMD, check=False)
            if rc == 1:
                raise Execption("BROWSER_CMD failed")
            if rc == 0:
                break
            time.sleep(1)
        out = out[0].split('\n')[-2].split(' ')[-1]
        rpc(MASTER_SERVER, 'browser_stats', (out,type))
        check_output('killall proxy.py')

    def exposed_run_webserver(self, type):
        myAD = self.exposed_get_ad()
        myHID = self.exposed_get_hid()
        neighbor = rpc(MASTER_SERVER, 'get_neighbor_host', ())
        my_gateway = rpc(MASTER_SERVER, 'get_gateway_host', (type, my_name))
        their_gateway = rpc(MASTER_SERVER, 'get_gateway_host', (type, neighbor))
        g4ID = rpc(my_gateway, 'get_fourid', ())
        gAD = rpc(their_gateway, 'get_ad', ())
        print myAD, myHID, neighbor, my_gateway, their_gateway, g4ID, gAD
        Popen('%s AD:%s HID:%s IP:%s AD:%s' % (WEBSERVER_CMD, myAD, myHID, g4ID, gAD),shell=True)
        time.sleep(3)

    def exposed_gather_stats(self):
        printtime('<<<<GATHER STATS>>>>')
        neighbors = rpc(MASTER_SERVER, 'get_backbone', ())
        out = multi_ping(neighbors)
        latency = out[0][0]
        my_backbone = out[0][1]
        hops = traceroute(my_backbone)
        rpc(MASTER_SERVER, 'stats', (my_backbone, latency, hops))
        return ['Sent stats: (%s, %s, %s)' % (my_backbone, latency, hops), my_backbone]

    def exposed_gather_xstats(self):
        printtime('<<<<GATHER XSTATS>>>>')
        neighbor = rpc(MASTER_SERVER, 'get_neighbor_xhost', ())
        printtime('neighbor: %s' % neighbor)
        xlatency = xping(neighbor)
        printtime('xlatency: %s' % xlatency)
        time.sleep(2)
        xhops = xtraceroute(neighbor)
        printtime('xhops: %s' % xhops)
        rpc(MASTER_SERVER, 'xstats', (xlatency, xhops))
        return 'Sent xstats: (%s, %s, %s)' % (neighbor, xlatency, xhops)

    def exposed_gather_fourid_stats(self, type): 
        printtime('<<<<GATHER FOURIDSTATS>>>>')
        # [CA, 6RDA, GA, SDNA, CB, 6RDB, GB, SDNB]
        fidn = rpc(MASTER_SERVER, 'get_fourid_nodes', ())
        print my_name, fidn
        if my_name not in fidn:
            return
        elif my_name in [fidn[0], fidn[4]]: #CA, CB
            printtime('<<<<<NODE STARTING 4IDSTAT>>>>')
            i = fidn.index(my_name)
            gpair = 'AG' if i == 0 else 'BG'
            npair = 'AB' if i == 0 else 'BA'
            neighbor = fidn[4] if i == 0 else fidn[0]

            #for type in ['SDN', '4ID', '6RD']:
            printtime('<<<<NODE STARTING %s STATS>>>>' % type)
            gateway = rpc(MASTER_SERVER,'get_gateway_host',(type,my_name))
#                 DST = rpc(MASTER_SERVER,'get_gateway_xhost',(type,))

#                 xlatency = xping(DST)
#                 xhops = xtraceroute(DST)
                
#            DST_h = rpc(MASTER_SERVER,'get_gateway_host',(type,my_name))
            latency = multi_ping([gateway])[0][0]
            hops = traceroute(gateway, gateway=True)
#                rpc(MASTER_SERVER, 'fidstats', ('%s-%s' % (type, gpair), gateway, xlatency, xhops))
            rpc(MASTER_SERVER, 'fidstats', ('%s-%s' % (type, gpair), gateway, latency, hops))

            SRC = rpc(MASTER_SERVER,'get_fourid_neighbor_xhost',(type,True))
            DST = rpc(MASTER_SERVER,'get_fourid_neighbor_xhost',(type,False))
            xlatency = xping(DST, src=SRC)
            xhops = -1
            rpc(MASTER_SERVER, 'fidstats', ('%s-%s' % (type, npair), neighbor, xlatency, xhops))

            if my_name == fidn[4]: #CB
                self.exposed_gather_browser_stats(type)
            elif my_name == fidn[0]: #CA
                self.exposed_run_webserver(type)
                while not rpc(MASTER_SERVER, 'check_done_type', (type,)):
                    print "Waiting for browser test to complete..."
                    time.sleep(1)

        elif my_name in [fidn[1], fidn[2], fidn[3]]: # 6RDA, GA, SDNA
            indices = [i for i,x in enumerate(fidn) if x == my_name]
            for i in indices:
                t = ['6RD', '4ID', 'SDN'][i-1]
                if t == type:
                    partner_node = fidn[i+4]
                    latency = multi_ping([partner_node])[0][0]
                    hops = traceroute(partner_node)
                    rpc(MASTER_SERVER, 'fidstats', ('%s-GG' % type, partner_node, latency, hops))

    def exposed_get_ping(self, nodes):
        return multi_ping(nodes, 5)

    def try_xroute(self):
        return check_output(XROUTE)

    def exposed_get_fourid(self):
        xr_out = self.try_xroute()
        return re.search(r'IP:(.*) *-2 \(self\)', xr_out[0]).group(1).strip().lower()

    def exposed_get_hid(self):
        xr_out = self.try_xroute()
        return re.search(r'HID:(.*) *-2 \(self\)', xr_out[0]).group(1).strip().lower()

    def exposed_get_ad(self):
        xr_out = self.try_xroute()
        return re.search(r'AD:(.*) *-2 \(self\)', xr_out[0]).group(1).strip().lower()

    def exposed_get_neighbors(self):
        xr_out = check_output(XROUTE)
        neighbors = []
        for xline in xr_out[0].split('\n'):
            try:
                neighbors.append(re.split(' *',xline)[4].split(':')[1])
            except:
                pass
        neighbors = list(set(neighbors))
        neighbors = [neighbor.lower() for neighbor in neighbors]
        myHID = self.exposed_get_hid()
        if myHID in neighbors: neighbors.remove(myHID)
        if BROADCAST_HID in neighbors: neighbors.remove(BROADCAST_HID)
        return neighbors

    def exposed_soft_restart(self, neighbor):
        xianetcmd = rpc(MASTER_SERVER, 'get_xianet', (neighbor, ))
        check_output(KILL_XIANET)
        printtime('running %s' % xianetcmd)
        exec(xianetcmd)
        return xianetcmd

    def exposed_run_commands(self):
        printtime('stopping local processes')
        printtime('requesting commands!')
        commands = rpc(MASTER_SERVER, 'get_commands', ())
        printtime('commands received!')
        printtime('commands: %s' % commands)
        for command in commands:
            printtime(command)
            exec(command)

    def exposed_wait_for_neighbors(self, neighbors, msg):
        while True:
            for neighbor in neighbors:
                try:
                    printtime('waiting on: %s' % neighbor)
                    out = rpc(neighbor, 'get_hid', ())
                    return out
                except:
                    printtime(msg)
                    time.sleep(1)

class Mapper(threading.Thread):
    def __init__(self, goOnEvent):
        super(Mapper, self).__init__()
        self.goOnEvent = goOnEvent

    def run(self):
        while self.goOnEvent.isSet():
            try:
                myHID = rpc('localhost', 'get_hid', ())
                neighbors = rpc('localhost', 'get_neighbors', ())
                rpc(MASTER_SERVER, 'heartbeat', (myHID, neighbors))
            except Exception, e:
                pass
            time.sleep(BEAT_PERIOD)


class Runner(threading.Thread):
    def run(self):
        while FINISH_EVENT.isSet():
            try:
                rpc('localhost', 'run_commands', ())
                break
            except Exception, e:
                printtime('%s' % e)
                #rpc(MASTER_SERVER, 'error', ('command broke?', my_name))
                time.sleep(1)

if __name__ == '__main__':
    printtime(('RPC server listening on port %d\n'
        'press Ctrl-C to stop\n') % RPC_PORT)

    if len(sys.argv) > 1:
        if sys.argv[1] == 'setup':
            FOURID_SETUP = True

    FINISH_EVENT.set()
    mapper = Mapper(goOnEvent = FINISH_EVENT)
    mapper.start()

    if not FOURID_SETUP:
        runner = Runner()
        runner.start()

    try:
        t = ThreadPoolServer(MyService, port = RPC_PORT)
        t.start()
    except Exception, e:
        printtime('%s' % e)

    printtime('Local_Server Exiting, please wait...')
    FINISH_EVENT.clear()
    mapper.join()

    if not FOURID_SETUP:
        runner.join()

    printtime('Local_Server Finished.')
