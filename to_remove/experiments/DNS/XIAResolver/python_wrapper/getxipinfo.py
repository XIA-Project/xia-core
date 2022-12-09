#! /usr/bin/env python
from ctypes import *

def XgetDAGbyname(name):
  libc = cdll.LoadLibrary("./XgetDAGbyname.so")
  libc.XgetDAGbyname.restype = c_char_p
  return libc.XgetDAGbyname(name)

if __name__ == '__main__':
  print "XIP lookup (type 'quit' to quit)"
  domain_name = raw_input("> ")
  print XgetDAGbyname(domain_name)
