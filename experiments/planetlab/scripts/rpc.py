#!/usr/bin/python

RPC_PORT = 43278
import rpyc

def rpc(dest, cmd, args):
    c = rpyc.connect(dest, RPC_PORT)
    out = c.root.cmd(*args)

