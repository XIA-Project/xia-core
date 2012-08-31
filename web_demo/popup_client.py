#!/usr/bin/python

from multiprocessing.connection import Client
from array import array
import struct, time, signal, os, sys, re
import threading
import Tkinter
from tkMessageBox import showwarning

address = ('pc251.emulab.net', 8081)
#address = ('155.98.39.51', 8081)
conn = Client(address, authkey='secret password')
print 'connected.'
while 1:
    msg = conn.recv()
    if not msg: break
    # do something with msg
    if msg == 'bad_content':
        print 'bad_content'
        thread = threading.Thread(target=os.system, args=("python bad_content_warning.py",))
        thread.start()
    elif msg == 'close':
        conn.close()
        break
conn.close()
