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
from socket import *

#try:
sock = socket(AF_INET, SOCK_DGRAM)

sock.bind(("0.0.0.0", 8888))

while (1):
	print "waiting for data"
	(data, addr) = sock.recvfrom(8192, 0)
	print "received %d bytes from client" % (len(data))
	sock.sendto(data, 0, addr)

sock.close()

#except:
#	print "socket error"

