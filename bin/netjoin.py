#!/usr/bin/python
#
# Copyright 2013 Carnegie Mellon University
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

import time
import socket

# Run by itself, we do a unit test by periodically sending some data
# down to Click and expecting to get it back.
if __name__ == "__main__":
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM);
    sock.bind(('127.0.0.1', 9228))
    it = 0;
    while True:
        it += 1
        message = 'Iteration: ' + str(it)
        sock.sendto(message, ('127.0.0.1', 9882))
        print 'Sent: ' + message
        reply, sender = sock.recvfrom(1024)
        print 'Got: %s' % reply
        time.sleep(1)
