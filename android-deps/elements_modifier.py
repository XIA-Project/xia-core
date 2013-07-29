#!/usr/bin/python
f = open('click-2.0.1/userlevel/elements.cc', 'r')
f2= open('click-2.0.1/userlevel/elementsMod.cc', 'w')
hotWord = "IPFlowRawSockets"
hotWord2 = "ipflowrawsockets.hh"
for line in f:
	if hotWord in line or hotWord2 in line:
		f2.write("#ifdef CLICK_ANDROID\n")
		f2.write("#else\n")
		f2.write(line)
		f2.write("#endif\n")
	else:
		f2.write(line)
