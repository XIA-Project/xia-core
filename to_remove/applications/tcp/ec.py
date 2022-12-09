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

# simple XIA echo client using datagram sockets

import sys
import os
from socket import *

if len(sys.argv) != 2:
	print "usage: ec hostname"
	sys.exit(1)

sock = socket(AF_INET, SOCK_DGRAM)

while (1):
	print "Please enter the message (blank line to exit):"
	text = sys.stdin.readline()
	text = text.strip()
	if (len(text) == 0):
		break
	try:
		sock.sendto(text, 0, (sys.argv[1], 8888))
	except:
		print sys.argv[1] + " was not found"
		sys.exit(1)

	(data, addr) = sock.recvfrom(8192, 0)
	print data

sock.close()

