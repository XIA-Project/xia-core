#!/usr/bin/python

import rpyc, commands, re

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

    def exposed_get_hid(self): # this is an exposed method
        xr_out = commands.getoutput(XROUTE)
        return re.search(r'HID:(.*) *-2 \(self\)', xr_out).group(1).strip().lower()

if __name__ == "__main__":
    from rpyc.utils.server import ThreadedServer
    t = ThreadedServer(MyService, port = 18861)
    t.start()
