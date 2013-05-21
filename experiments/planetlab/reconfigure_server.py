#!/usr/bin/python

"""Threaded heartbeat server"""

UDP_PORT = 5691; CHECK_TIMEOUT = 5; CHECK_PERIOD = 5
CLIENT_PORT = '3000';
KILL_CMD = 'sudo ~/fedora-bin/xia-core/bin/xianet stop'


import socket, threading, commands, sys, time
from subprocess import call

class Receiver(threading.Thread):

    def __init__(self, goOnEvent, cmd):
        super(Receiver, self).__init__()
        self.goOnEvent = goOnEvent
        self.cmd = cmd
        self.recSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.recSocket.settimeout(CHECK_TIMEOUT)
        self.recSocket.bind(('', UDP_PORT))

    def run(self):
        while self.goOnEvent.isSet():
            try:
                data, addr = self.recSocket.recvfrom(65535)
                data = data.split(':');
                if data[0] == 'PyRestart':
                    if len(self.cmd[1].split(',')) < 4:
                        ip = data[1]
                        run = self.cmd[0] + ' ' + self.cmd[1].strip() + ',' + ip + ':' + CLIENT_PORT + ' ' + self.cmd[2]
                        call(KILL_CMD,shell=True)                        
                        print run
                        call(run,shell=True)
            except socket.timeout:
                pass
            except KeyError:
                pass

def main():
    my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]
    my_commands = []

    if len(sys.argv) < 2:
        print "Usage: %s [path/to/cmd_file]" % sys.argv[0]
        sys.exit(-1)

    cmd_file = sys.argv[1]
    try: 
        f = open(cmd_file, 'r')
        sections = f.read().split('[')
        for section in sections:
            ip = section.split(']')[0]
            if ip == 'default':
                my_commands += section.split('\n')[1:-1]
            if ip == my_ip:
                    my_commands += section.split('\n')[1:-1]
        f.close()
    except Exception, e: 
        print e
    
    cmd = []
    cmd.append(my_commands[-1].split('-P')[0] + '-P')
    cmd.append(my_commands[-1].split('-P')[1].split('-f')[0])
    cmd.append('-f' + my_commands[-1].split('-P')[1].split('-f')[1])

    receiverEvent = threading.Event()
    receiverEvent.set()
    receiver = Receiver(goOnEvent = receiverEvent, cmd = cmd)
    receiver.start()
    print ('Threaded server listening on port %d\n'
        'press Ctrl-C to stop\n') % UDP_PORT
    try:
        while True:
            print 'Listening'
            time.sleep(CHECK_PERIOD)
    except KeyboardInterrupt:
        print 'Exiting, please wait...'
        receiverEvent.clear()
        receiver.join()
        print 'Finished.'

if __name__ == '__main__':
    main()
