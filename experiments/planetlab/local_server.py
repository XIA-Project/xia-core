#!/usr/bin/python

import rpyc, commands, re
from subprocess import call
from rpyc.utils.server import ThreadedServer

RPC_PORT = 5691;
CLIENT_PORT = '3000';
KILL_CMD = 'sudo ~/fedora-bin/xia-core/bin/xianet stop'
XROUTE = '/home/cmu_xia/fedora-bin/xia-core/bin/xroute -v'
my_ip = commands.getoutput("/sbin/ifconfig").split("\n")[1].split()[1][5:]
myHID = ''

class MyService(rpyc.Service):
    def on_connect(self):
        # code that runs when a connection is created
        # (to init the serivce, if needed)
        pass

    def on_disconnect(self):
        # code that runs when the connection has already closed
        # (to finalize the service, if needed)
        pass

    def exposed_get_hid(self):
        xr_out = commands.getoutput(XROUTE)
        return re.search(r'HID:(.*) *-2 \(self\)', xr_out).group(1).strip().lower()

    def exposed_get_ad(self):
        xr_out = commands.getoutput(XROUTE)
        return re.search(r'AD:(.*) *-2 \(self\)', xr_out).group(1).strip().lower()

    def exposed_restart(self, cmd_file, remote_ip):
        my_commands = []
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

        if len(cmd[1].split(',')) < 4:
            run = cmd[0] + ' ' + cmd[1].strip() + ',' + remote_ip + ':' + CLIENT_PORT + ' ' + cmd[2]
            call(KILL_CMD,shell=True)                        
            print run
            call(run,shell=True)

if __name__ == '__main__':
    print ('RPC server listening on port %d\n'
        'press Ctrl-C to stop\n') % RPC_PORT

    t = ThreadedServer(MyService, port = RPC_PORT)
    t.start()

