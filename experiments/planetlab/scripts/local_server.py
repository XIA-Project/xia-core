#!/usr/bin/python

import rpyc, re, time, threading
from check_output import check_output
from rpc import rpc
from subprocess import call
from rpyc.utils.server import ThreadedServer

RPC_PORT = 43278
KILL_XIANET = 'sudo ~/fedora-bin/xia-core/bin/xianet stop'
XROUTE = '/home/cmu_xia/fedora-bin/xia-core/bin/xroute -v'
MASTER_NAME = 'GS11698.SP.CS.CMU.EDU'
BEAT_PERIOD = 3
BROADCAST_HID = 'ffffffffffffffffffffffffffffffffffffffff'
PING_INTERVAL = .25
PING_COUNT = 4

myHID = ''
my_name = check_output("hostname")

finishEvent = threading.Event()


def multi_ping(neighbors):
    pingcmd = 'sudo ping -W 1 -i %s -c' % PING_INGERVAL
    [check_output('%s %s %s' % (pingcmd, 1, node)) for neighbor in neighbors]
    outs = [check_output('%s %s %s' % (pingcmd, PING_COUNT, node)) for neighbor in neighbors]

    stats = []
    for out in outs:
        host = out[0].split('\n')[0].split(' ')[1]
        p = float(out[0].split("\n")[-2].split('=')[1].split('/')[1])
        stats.append((p,host))
    stats = sorted(stats)
    stats = [("%.3f" % stat[0], stat[1]) for stat in stats]
    return stats

def traceroute(neighbor):
    out = check_output('sudo traceroute -I -w 1 %s' % neighbor)
    stat = int(out[0].split("\n")[-2].strip().split(' ')[0])
    stat = -1 if stat is 30 else stat
    return stat

def xping(neighbor):
    xpingcmd = '/home/cmu_xia/fedora-bin/xia-core/bin/xping -i %s -c' % PING_INTERVAL
    check_output('%s %s %s' % (xpingcmd, 1, neighbor))
    out = check_output('%s %s %s' % (xpingcmd, PING_COUNT, neighbor))
    stat = (float(out[0].split("\n")[-2].split('=')[1].split('/')[1]), out[0].split('\n')[0].split(' ')[1])
    stat = ("%.3f" % stat[0], stat[1])
    return stat

def xtraceroute(neighbor):
    out = check_output('/home/cmu_xia/fedora-bin/xia-core/bin/xtraceroute %s' % neighbor)
    stat = int(out[0].split('\n')[-2].split('=')[1].strip())
    stat = -1 if stat is 30 else stat
    return stat
    
class MyService(rpyc.Service):
    def on_connect(self):
        # code that runs when a connection is created
        # (to init the serivce, if needed)
        pass

    def on_disconnect(self):
        # code that runs when the connection has already closed
        # (to finalize the service, if needed)
        pass

    def exposed_gather_stats(self):
        neighbors = rpc(MASTER_SERVER,get_backbone, ())
        out = multi_ping(neighbors)
        latency = out[0][0]
        my_backbone = out[0][1]
        hops = traceroute(my_backbone)
        rpc(MASTER_SERVER, stats, (my_backbone, latency, hops))
        return ['Sent stats: (%s, %s, %s)' (my_backbone, latency, hops), my_backbone]

    def exposed_gather_xstats(self):
        neighbor = rpc(MASTER_SERVER, get_neighbor, ())
        xlatency = xping(neighbor)
        xhops = xtraceroute(neighbor)
        rpc(MASTER_SERVER, xstats, (xlatency, xhops))
        return 'Sent xstats: (%s, %s, %s)' (neighbor, xlatency, xhops)

    def exposed_get_hid(self):
        xr_out = check_output(XROUTE)
        return re.search(r'HID:(.*) *-2 \(self\)', xr_out).group(1).strip().lower()

    def exposed_get_ad(self):
        xr_out = check_output(XROUTE)
        return re.search(r'AD:(.*) *-2 \(self\)', xr_out).group(1).strip().lower()

    def exposed_get_neighbors(self):
        xr_out = check_output(XROUTE)
        neighbors = []
        for xline in xr_out.split('\n'):
            try:
                neighbors.append(re.split(' *',xline)[4].split(':')[1])
            except:
                pass
        neighbors = list(set(neighbors))
        neighbors = [neighbor.lower() for neighbor in neighbors]
        myHID = self.exposed.get_hid()
        if myHID in neighbors: neighbors.remove(myHID)
        if BROADCAST_HID in neighbors: neighbors.remove(BROADCAST_HID)
        return neighbors

    def exposed_soft_restart(self, neighbor):
        xianetcmd = rpc(MASTER_SERVER, get_xianet, (neighbor))
        check_output(KILL_XIANET)
        out = check_output(run)
        return '%s: %s' % (xianetcmd, out)

    def exposed_wait_for_neighbor(self, neighbor, msg):
        out = None
        while out is None:
            try:
                out = rpc(neighbor, get_hid, ())
            except:
                print msg
                time.sleep(1)
        return out

    def exposed_hard_stop(self):
        kill_cmd = rpc(MASTER_SERVER, get_kill, ())
        call(kill_cmd)
        sys.exit(0)


class Mapper(threading.Thread):
    def __init__(self, goOnEvent):
        super(Mapper, self).__init__()
        self.goOnEvent = goOnEvent

    def run(self):
        while self.goOnEvent.isSet():
            try:
                myHID = rpc('localhost', get_hid, ())
                neighbors = rpc('localhost', get_neighbors, ())
                rpc(MASTER_SERVER, heartbeat, (myHID, neighbors))
            except:
                pass
            time.sleep(BEAT_PERIOD)


class Runner(threading.Thread):
    def run(self):
        try:
            commands = rpc(MASTER_SERVER, get_commands, ())
            for command in commands:
                eval(command)
        except:
            rpc(MASTER_SERVER, error, ('Runner'))

if __name__ == '__main__':
    print ('RPC server listening on port %d\n'
        'press Ctrl-C to stop\n') % RPC_PORT

    finishEvent.set()
    mapper = Mapper(goOnEvent = finishEvent)
    mapper.start()

    runner = Runner()
    runner.start()

    try:
        t = ThreadedServer(MyService, port = RPC_PORT)
        t.start()
    except:
        rpc(MASTER_SERVER, error, ('RPC Server'))

    print 'Exiting, please wait...'
    finishEvent.clear()
    mapper.join()
    runner.join()

    print 'Finished.'
