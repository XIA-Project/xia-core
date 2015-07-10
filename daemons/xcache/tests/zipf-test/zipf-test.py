#!/usr/bin/python

import numpy as np
from subprocess import call


count = 1000
s = set()

while True:
    cid = np.random.zipf(2.1)
    if cid in s:
        print("searching " + str(cid))
        call(["../xcache_client", "search", str(cid)])
    else:
        print("Storing " + str(cid))
        call(["../xcache_client", "store", str(cid), "./data", "2000"])
        s.add(cid)
    count = count - 1
    if count == 0:
        break
