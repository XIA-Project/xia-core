#!/usr/bin/python

RPC_PORT = 43278
import rpyc

def rpc(dest, cmd, args):
    c = rpyc.connect(dest, RPC_PORT)
    s = 'c.root.%s(*args)' % cmd
    out = eval(s)
    del c
    return out
