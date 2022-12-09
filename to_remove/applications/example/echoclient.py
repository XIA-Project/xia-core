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

# simple XIA echo client using stream sockets

import sys
import os

# find the path to xia-core
XIADIR=os.getcwd()
while os.path.split(XIADIR)[1] != 'xia-core':
    XIADIR=os.path.split(XIADIR)[0]
sys.path.append(XIADIR + '/api/lib')

from c_xsocket import *

STREAM_NAME = "www_s.stream_echo.aaa.xia"

try:
	sock = Xsocket(XSOCK_STREAM)
	dag = XgetDAGbyName(STREAM_NAME)
	Xconnect(sock, dag)

	while (1):
		print "Please enter the message (blank line to exit):"
		text = sys.stdin.readline()
		text = text.strip()
		if (len(text) == 0):
			break
		Xsend(sock, text, 0)

		data = Xrecv(sock, 8192, 0)
		print data

	Xclose(sock)

except:
	print "socket error"

