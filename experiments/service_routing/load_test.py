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
from subprocess import PIPE
import time
import re

n1 = None
LOAD = 200
processes_id = []
processes = []
elapsed_times = []

def report_elapsed_time():
    for p in processes:
        out = p.communicate()[0]
        if elapsed_re.search(out):
            elapsed_time = elapsed_re.search(out).group(1)
            if elapsed_time:
                elapsed_times.append(int(elapsed_time))
    print len(elapsed_times), "clients finished"
    print "Elapsed times: min/max/avg: %d / %d / %d" % (min(elapsed_times), 
        max(elapsed_times), sum(elapsed_times)/len(elapsed_times))

def signal_handler(s, frame):
    for this_pid in processes_id:
        try:
            os.kill(this_pid, s)
        except OSError, err:
            pass
    report_elapsed_time()
    exit()

if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    elapsed_re = re.compile(r'Elapsed time: (\d+)')
    full_path = os.path.realpath(__file__)
    os.chdir(os.path.dirname(full_path)+"/../..")

    n1 = time.time()
    for i in xrange(LOAD):
        p = subprocess.Popen(
            'bin/xwrap applications/tinyhttpd/simpleclient', 
            #'bin/xwrap applications/tinyhttpd/simpleclient_orig', 
            shell=True, stdout=PIPE, stderr=PIPE)
        processes.append(p)
        processes_id.append(p.pid)
    finished = 0
    while processes_id:
        try:
            pid, retval = os.wait()
            finished += 1
            #print '%d/%d finished' % (finished, LOAD)
            processes_id.remove(pid)
        except OSError, err:
            break

    n2 = time.time()
    total_time = (n2-n1)
    print total_time, 'seconds'
    #print total_time/LOAD*1000, 'ms per request'
    report_elapsed_time()
