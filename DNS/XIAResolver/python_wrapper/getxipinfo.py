#! /usr/bin/env python
from ctypes import *

class xipinfo(Structure):
	pass
xipinfo._fields_ = [("dag", c_char*256), ("next", POINTER(xipinfo))]

def xiplookup(name):
	libc = cdll.LoadLibrary("./getXIPinfo.so")
	result = cast(0, POINTER(xipinfo))
	ret = libc.getXIPinfo(pointer(result), name)
	if ret < 0:
		return []
	daglist = []
	while True:
		daglist.append(result.contents.dag)
		result = result.contents.next
		try:
			result.contents
		except ValueError:
			break
	return daglist 

if __name__ == '__main__':
	print "XIP lookup (type 'quit' to quit)"
	domain_name = raw_input("> ")
	print xiplookup(domain_name)[0]
