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
PING_INTERVAL = .25
XPING_INTERVAL = 1
PING_COUNT = 4

myHID = ''
my_name = check_output("hostname")[0].strip()

FINISH_EVENT = threading.Event()    

FOURID_SETUP = False

def multi_ping(neighbors):
    print "<<<< Multi Ping >>>>"
    pingcmd = 'sudo ping -W 1 -i %s -c' % PING_INTERVAL
    
    processes = [(Popen('%s %s %s' % (pingcmd, 1, neighbor), shell=True, stdout=PIPE, stderr=PIPE), neighbor) for neighbor in neighbors]
    outs = [process[0].communicate() for process in processes]

    processes = [(Popen('%s %s %s' % (pingcmd, PING_COUNT, neighbor), shell=True, stdout=PIPE, stderr=PIPE), neighbor) for neighbor in neighbors]
    outs = [process[0].communicate() for process in processes]
    rcs = [process[0].wait() for process in processes]
    outs = zip(outs, rcs, [p[1] for p in processes])

    stats = []
    for out in outs:
            p = float(out[0][0].split("\n")[-2].split('=')[1].split('/')[1]) if out[1] == 0 else 5000.00
            stats.append((p,out[2]))
    stats = sorted(stats)
    stats = [(-1,stat[1]) if stat[0] == 5000 else stat for stat in stats]
    stats = [("%.3f" % stat[0], stat[1]) for stat in stats]
    return stats

def traceroute(neighbor):
    out = check_output('sudo traceroute -I -w 1 %s' % neighbor)
    print out
    stat = int(out[0].split("\n")[-2].strip().split(' ')[0])
    stat = -1 if stat is 30 else stat
    return stat

def xping(neighbor,tryUntilSuccess=True,src=None):
    xpingcmd = '/home/cmu_xia/fedora-bin/xia-core/bin/xping -t 5'
    if src:
        xpingcmd = '%s -s "%s"' % (xpingcmd, src)
    while tryUntilSuccess:
        s = '%s "%s"' % (xpingcmd, neighbor)
        while True:
            try:
                printtime(s)
                out = check_output(s)
                printtime(out)
                stat = "%.3f" % float(out[0].split("\n")[-2].split('=')[1].split('/')[1])
                break
            except:
                pass
        s = '%s -c %s "%s"' % (xpingcmd, PING_COUNT, neighbor)
        while tryUntilSuccess:
            try:
                printtime(s)
                out = check_output(s)
                break
            except:
                pass
        printtime(out)
        try:
            stat = "%.3f" % float(out[0].split("\n")[-2].split('=')[1].split('/')[1])
            break
        except:
            stat = -1
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

    def exposed_gather_browser_stats(self):
        # write to etc/hosts.xia
        check_output('echo "%s %s" > %s' % (BROWSER_ADDR, rpc(MASTER_SERVER, 'get_neighbor_webserver', ()), ETC_HOSTS))

        print check_output('cat %s' % ETC_HOSTS)

        while True:
            try:
                check_output('ps -e | grep proxy.py')
                break
            except:
                Popen(PROXY_CMD, shell=True)
                time.sleep(1)
        time.sleep(3)
        p = Popen(BROWSER_CMD,shell=True)
        p.wait()
        time.sleep(3)
        while True:
            (out, rc) = check_both(BROWSER_CMD, check=False)
            if rc == 1:
                raise Execption("BROWSER_CMD failed")
            if rc == 0:
                break
        out = out[0].split('\n')[-2].split(' ')[-1]
        rpc(MASTER_SERVER, 'browser_stats', (out,))

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

#     class fourid_stat_runner(threading.Thread):
#         def __init__(self, bucket, type, gpair, npair, neighbor):
#             threading.Thread.__init__(self)
#             self.bucket = bucket
#             self.type = type
#             self.gpair = gpair
#             self.npair = npair
#             self.neighbor = neighbor
#         def run(self):
#             try:
#                 printtime('<<<<NODE STARTING %s STATS>>>>' % self.type)
#                 gateway = rpc(MASTER_SERVER,'get_gateway_host',(self.type,))
#                 DST = rpc(MASTER_SERVER,'get_gateway_xhost',(self.type,))
#                 xlatency = xping(DST)
#                 xhops = xtraceroute(DST)
#                 rpc(MASTER_SERVER, 'fidstats', ('%s-%s' % (self.type, self.gpair), gateway, xlatency, xhops))

#                 SRC = rpc(MASTER_SERVER,'get_fourid_neighbor_xhost',(self.type,True))
#                 DST = rpc(MASTER_SERVER,'get_fourid_neighbor_xhost',(self.type,False))
#                 xlatency = xping(DST, src=SRC)
# #####
#                 #xhops = xtraceroute(DST, src=SRC)
#                 xhops = -1
# ####
#                 rpc(MASTER_SERVER, 'fidstats', ('%s-%s' % (self.type, self.npair), self.neighbor, xlatency, xhops))
#             except:
#                 self.bucket.put(sys.exc_info())

    def exposed_gather_fourid_stats(self): 
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

#            buckets = [Queue.Queue(), Queue.Queue(), Queue.Queue()]
#            threads = []
#            types = ['6RD', '4ID', 'SDN']
            for type in ['SDN', '4ID', '6RD']:
                printtime('<<<<NODE STARTING %s STATS>>>>' % type)
                gateway = rpc(MASTER_SERVER,'get_gateway_host',(type,))
#                 DST = rpc(MASTER_SERVER,'get_gateway_xhost',(type,))

#                 xlatency = xping(DST)
#                 xhops = xtraceroute(DST)
                
                DST_h = rpc(MASTER_SERVER,'get_gateway_host',(type,))
                latency = multiping([DST_h])[0][0]
                hops = traceroute(DST_h)
#                rpc(MASTER_SERVER, 'fidstats', ('%s-%s' % (type, gpair), gateway, xlatency, xhops))
                rpc(MASTER_SERVER, 'fidstats', ('%s-%s' % (type, gpair), gateway, latency, hops))

                SRC = rpc(MASTER_SERVER,'get_fourid_neighbor_xhost',(type,True))
                DST = rpc(MASTER_SERVER,'get_fourid_neighbor_xhost',(type,False))
                xlatency = xping(DST, src=SRC)
#####
                #xhops = xtraceroute(DST, src=SRC)
                xhops = -1
####
                rpc(MASTER_SERVER, 'fidstats', ('%s-%s' % (type, npair), neighbor, xlatency, xhops))
#             for i in range(3):
#                 threads.append(fourid_stat_runner(bucket[i], type[i], gpair, npair, neighbor))
#                 threads[i].start()
#             while True:
#                 for i in range(3):
#                     try:
#                         exc = buckets[i].get(block=False)
#                         break
#                     except Queue.Empty:
#                         pass
#                 if exc:
#                     rpc(MASTER_SERVER, error, ('4ID Stats', my_name))
#                 for i in range(3):
#                     threads[i].join(0.1)
#                 if not threads[0].isAlive() and not thread[1].isAlive() and not thread[2].isAlive():
#                     break

        elif my_name in [fidn[1], fidn[2], fidn[3]]: # 6RDA, GA, SDNA
            indices = [i for i,x in enumerate(fidn) if x == my_name]
            for i in indices:
                type = ['6RD', '4ID', 'SDN'][i-1]
                partner_node = fidn[i+4]
                latency = multi_ping([partner_node])[0][0]
                hops = traceroute(partner_node)
                rpc(MASTER_SERVER, 'fidstats', ('%s-GG' % type, partner_node, latency, hops))

    def exposed_get_ping(self, nodes):
        return multi_ping(nodes)

    def try_xroute(self):
#         i = 0
#         while i < 5:
#             try:
#                 xr_out = check_output(XROUTE)
#                 break
#             except:
#                 self.exposed_soft_restart(None)
#                 i += 1
#         if i < 5:
#             return xr_out
#         else:
#             raise Exception("Failed to restart XIANET")
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
