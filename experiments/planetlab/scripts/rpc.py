#!/usr/bin/python

import copy 
import rpyc

RPC_PORT = 43278

def rpc(dest, cmd, args):
    c = rpyc.connect(dest, RPC_PORT)
    s = 'c.root.%s(*args)' % cmd
    out = copy.deepcopy(eval(s))
    del c
    return out
