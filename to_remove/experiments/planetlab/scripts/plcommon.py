#!/usr/bin/python

import copy, time, threading, rpyc, sys
from threading import Thread
from subprocess import Popen, PIPE, STDOUT

RPC_PORT = 43278
CHECK_TIMEOUT = 15

def stime():
    return time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime())

def printtime(s):
    print '%s: %s' % (stime(), s)

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
            if len(val) is 2:
                return val[1]
            return val[1:]
        raise Exception("KeyError: " + key)            

    def getClients(self):
        limit = time.time() - CHECK_TIMEOUT
        self._lock.acquire()
        clients = [val[1:]+[host] for (host, val) in self.items() if val[0] >= limit]
        self._lock.release()
        return clients

def rpc(dest, cmd, args):
    c = rpyc.connect(dest, RPC_PORT)
    s = 'c.root.%s(*args)' % cmd
    out = copy.deepcopy(eval(s))
    c.close()
    return out

def check_output(args, shouldPrint=True):
    return check_both(args, shouldPrint)[0]

def check_both(args, shouldPrint=True, check=True):
    out = ""
    p = Popen(args,shell=True,stdout=PIPE,stderr=STDOUT)
    while True:
        line = p.stdout.readline()
        if not line:
            break
        if shouldPrint: sys.stdout.write(line)
        out += line
    rc = p.wait()
    out = (out,"")
    out = (out, rc)
    if check and rc is not 0:
        print "Error processes output: %s" % (out,)
        raise Exception("subprocess.CalledProcessError: Command '%s'" \
                            "returned non-zero exit status %s" % (args, rc))
    return out
