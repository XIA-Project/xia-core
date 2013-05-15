#!/usr/bin/python
# Filename: ThreadedBeatServer.py

"""Threaded heartbeat server"""

UDP_PORT = 43278; CHECK_PERIOD = 3; CHECK_TIMEOUT = 5

import socket, threading, time

class Heartbeats(dict):
    """Manage shared heartbeats dictionary with thread locking"""

    def __init__(self):
        super(Heartbeats, self).__init__()
        self._lock = threading.Lock()

    def __setitem__(self, key, value):
        """Create or update the dictionary entry for a client"""
        self._lock.acquire()
        super(Heartbeats, self).__setitem__(key, value)
        self._lock.release()

    def getSilent(self):
        """Return a list of clients with heartbeat older than CHECK_TIMEOUT"""
        limit = time.time() - CHECK_TIMEOUT
        self._lock.acquire()
        silent = [ip for (ip, ipTime) in self.items() if ipTime < limit]
        self._lock.release()
        return silent

    def getClients(self):
        limit = time.time() - CHECK_TIMEOUT
        self._lock.acquire()
        clients = [val[1:] for (ip, val) in self.items() if val[0] >= limit]
        self._lock.release()
        return clients

class Receiver(threading.Thread):
    """Receive UDP packets and log them in the heartbeats dictionary"""

    def __init__(self, goOnEvent, heartbeats):
        super(Receiver, self).__init__()
        self.goOnEvent = goOnEvent
        self.heartbeats = heartbeats
        self.recSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.recSocket.settimeout(CHECK_TIMEOUT)
        self.recSocket.bind(('', UDP_PORT))
        self.latlonfile = open('IPLATLON', 'r').read().split('\n')
        self.latlond = {}
        for ll in self.latlonfile:
            ll = ll.split(' ')
            self.latlond[ll[0]] = ll[1:]

    def run(self):
        while self.goOnEvent.isSet():
            try:
                data, addr = self.recSocket.recvfrom(30)
                data = data.split(':');
                if data[0] == 'PyHB':
                    ip = data[1].split(' ')[0]
                    color = data[1].split(' ')[1]
                    lat = self.latlond[ip][0]
                    lon = self.latlond[ip][1]
                    name = self.latlond[ip][2]
                    self.heartbeats[ip] = [time.time(), color, lat, lon, name]
            except socket.timeout:
                pass
            except KeyError:
                pass

def main():
    receiverEvent = threading.Event()
    receiverEvent.set()
    heartbeats = Heartbeats()
    receiver = Receiver(goOnEvent = receiverEvent, heartbeats = heartbeats)
    receiver.start()
    print ('Threaded heartbeat server listening on port %d\n'
        'press Ctrl-C to stop\n') % UDP_PORT
    try:
        while True:
            clients = heartbeats.getClients()
            print 'Active clients: %s' % clients
            time.sleep(CHECK_PERIOD)
    except KeyboardInterrupt:
        print 'Exiting, please wait...'
        receiverEvent.clear()
        receiver.join()
        print 'Finished.'

if __name__ == '__main__':
    main()
