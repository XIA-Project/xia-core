#!/usr/bin/python

import rpyc, re, time, threading, thread
from plcommon import check_output, rpc, printtime
from subprocess import call, Popen, PIPE
from rpyc.utils.server import ThreadPoolServer

RPC_PORT = 43278
KILL_XIANET = 'sudo ~/fedora-bin/xia-core/bin/xianet stop'
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

def multi_ping(neighbors):
    pingcmd = 'sudo ping -W 1 -i %s -c' % PING_INTERVAL
    
    processes = [Popen('%s %s %s' % (pingcmd, 1, neighbor), shell=True, stdout=PIPE, stderr=PIPE) for neighbor in neighbors]
    outs = [process.communicate() for process in processes]

    processes = [Popen('%s %s %s' % (pingcmd, PING_COUNT, neighbor), shell=True, stdout=PIPE, stderr=PIPE) for neighbor in neighbors]
    outs = [process.communicate() for process in processes]
    rcs = [process.wait() for process in processes]
    outs = zip(outs, rcs)

    stats = []
    for out in outs:
        host = out[0][0].split('\n')[0].split(' ')[1]
        p = float(out[0][0].split("\n")[-2].split('=')[1].split('/')[1]) if out[1] == 0 else 5000.00
        stats.append((p,host))
    stats = sorted(stats)
    stats = [(-1,stat[1]) if stat[0] == 5000 else stat for stat in stats]
    stats = [("%.3f" % stat[0], stat[1]) for stat in stats]
    return stats

def traceroute(neighbor):
    out = check_output('sudo traceroute -I -w 1 %s' % neighbor)
    stat = int(out[0].split("\n")[-2].strip().split(' ')[0])
    stat = -1 if stat is 30 else stat
    return stat

def xping(neighbor,tryUntilSuccess=True):
    while tryUntilSuccess:
        xpingcmd = '/home/cmu_xia/fedora-bin/xia-core/bin/xping -t 5'
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
        xpingcmd = '/home/cmu_xia/fedora-bin/xia-core/bin/xping -t 5 -i %s -c' % XPING_INTERVAL
        s = '%s %s "%s"' % (xpingcmd, PING_COUNT, neighbor)
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

def xtraceroute(neighbor,tryUntilSuccess=True):
    cmd = '/home/cmu_xia/fedora-bin/xia-core/bin/xtraceroute -t 30 "%s"' % neighbor
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
#########time.sleep(2)
        xhops = xtraceroute(neighbor)
        printtime('xhops: %s' % xhops)
        rpc(MASTER_SERVER, 'xstats', (xlatency, xhops))
        return 'Sent xstats: (%s, %s, %s)' % (neighbor, xlatency, xhops)

    def exposed_get_hid(self):
        xr_out = check_output(XROUTE)
        return re.search(r'HID:(.*) *-2 \(self\)', xr_out[0]).group(1).strip().lower()

    def exposed_get_ad(self):
        xr_out = check_output(XROUTE)
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
        printtime('requesting commands!')
        commands = rpc(MASTER_SERVER, 'get_commands', ())
        printtime('commands received!')
        printtime('commands: %s' % commands)
        for command in commands:
            printtime(command)
            exec(command)

    def exposed_wait_for_neighbor(self, neighbor, msg):
        while True:
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

    FINISH_EVENT.set()
    mapper = Mapper(goOnEvent = FINISH_EVENT)
    mapper.start()

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
    runner.join()

    printtime('Local_Server Finished.')
