#!/usr/bin/env python
#ts=4
#
# Copyright 2014 Carnegie Mellon University
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

import signal
import sys
import os
import subprocess
import time

n1 = None
LOAD = 100
processes = []

def signal_handler(s, frame):
    for this_pid in processes:
        os.kill(this_pid, s)

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)

    full_path = os.path.realpath(__file__)
    os.chdir(os.path.dirname(full_path)+"/../..")

    n1 = time.time()
    for i in xrange(LOAD):
        processes.append(subprocess.Popen(
            'bin/xwrap applications/tinyhttpd/simpleclient', 
            #'bin/xwrap applications/tinyhttpd/simpleclient_orig', 
            shell=True).pid)
    while processes:
        try:
            pid, retval = os.wait()
            print '%d/%d finished' % (LOAD - len(processes), LOAD)
            processes.remove(pid)
        except OSError, err:
            break
        
    n2 = time.time()
    total_time = (n2-n1)
    print total_time, 'seconds'
    print total_time/LOAD*1000, 'ms per request'
