#!/usr/bin/python
#ts=4
#
# Copyright 2012 Carnegie Mellon University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# simple XIA echo server using datagram sockets

import sys
import os

# find the path to xia-core
XIADIR=os.getcwd()
while os.path.split(XIADIR)[1] != 'xia-core':
    XIADIR=os.path.split(XIADIR)[0]
sys.path.append(XIADIR + '/api/lib')

from c_xsocket import *

NAME = "www_s.dgram_echo.aaa.xia"

try:
	SID = XmakeNewSID()

	sock = Xsocket(XSOCK_DGRAM)

	(ad, hid, fid) = XreadLocalHostAddr(sock)

	dag = "RE %s %s %s" % (ad, hid, SID) 

	XregisterName(NAME, dag)

	Xbind(sock, dag)

	while (1):
		print "waiting for data"
		(data, cdag) = Xrecvfrom(sock, 8192, 0)
		print "received %d bytes from client" % (len(data))
		Xsendto(sock, data, 0, cdag)

	Xclose(sock)
	XremoveSID(SID)

except:
	print "socket error"

