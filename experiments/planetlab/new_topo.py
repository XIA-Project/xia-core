#!/usr/bin/python

import rpyc

c = rpyc.connect('localhost', 43278)
print c.root.newtopo()

