#!/usr/bin/python
#
# Copyright 2011-2017 Carnegie Mellon University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#	http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import sys
import telnetlib

# default Click host and port (can be changed on cmd line)
HOST="localhost"
PORT=7777

# Minimum Click version required
MAJOR=1
MINOR=3

class clicksock:
	''' Generic interface to the click control socket'''

	#
	# connect to the click control socket
	#
	def __init__(self, host=HOST, port=PORT):
		try:
			self.csock = telnetlib.Telnet(host, port)
			self.connected = True

			data = self.csock.read_until("\n")
		except:
			self.errorExit("Unable to connect to Click")

		# make sure it's really click we're talking to
		data = data.strip()
		[name, ver] = data.split("/")
		[major, minor] = ver.split(".")
		if name != "Click::ControlSocket":
			self.errorExit("Socket is not a click ControlSocket")
		if int(major) < MAJOR or (int(major) == MAJOR and int(minor) < MINOR):
			self.errorExit("Click version %d.%d or higher is required" % (MAJOR, MINOR))


	def __enter__(self):
		return self

	def __exit__(self, type, value, traceback):
		if self.connected:
			self.shutdown()

	#
	# print an error message and exit the app with an error
	#
	def errorExit(self, msg):
		print msg
		self.shutdown()
		sys.exit(-1)

	#
	# get the ciick statuscode and message
	# some operations get 2 lines of status message with the code on each
	# and the second line is more useful, so the caller can specify if we should
	# die on error, or keep going and loop back for the 2nd line
	#
	def checkStatus(self, die):
		rc = self.csock.read_until("\n")

		# some result code lines are in the form of 'nnn msg' and some are nnn-msg'
		# so ignore the odd character by slicing round it
		code = int(rc[:3])
		msg = rc[4:-1]
		if (die and code != 200):
			self.errorExit("error %d: %s" % (code, msg))
		return code

	#
	# read the length of data sent by click so we can consume the right
	# amout of text
	#
	def readLength(self):
		text = self.csock.read_until("\n")
		text.strip()
		(data, length) = text.split()
		if data != "DATA":
			self.errorExit(tn, "error retreiving data length")
		return int(length)

	#
	# send a read command to click and return the resulting text
	#
	def readData(self, cmd):
		self.csock.write("READ %s\n" % (cmd))
		self.checkStatus(True)

		length = self.readLength()
		buf=""
		while len(buf) < length:
			buf += self.csock.read_some()
		return buf

	#
	# send a write command to click and verify it worked OK
	#
	def writeData(self, cmd):
		self.csock.write("WRITE %s\n" % (cmd))
		code = self.checkStatus(False)

		# the click write handler returns 2 lines of status on an error, and the
		# second line contains a more useful message, so call it again
		if code != 200:
			self.checkStatus(True)

	#
	# close the connection to click
	#
	def shutdown(self):
		if (self.connected):
			self.csock.write("quit\n")
			self.csock.close()
			self.connected = False


if __name__ == "__main__":
	c = clicksock()
	print c.readData("config")
