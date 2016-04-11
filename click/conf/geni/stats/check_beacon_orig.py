#!/usr/bin/python

import getpass
import sys
import telnetlib
import time

HOST = "localhost"
PORT = 7000
CNT_INDEX= 2

prev_total = 0

idle = 0

while True:
	time.sleep(1)
	
	try:
  		tn = telnetlib.Telnet(HOST, PORT)
  
  		tn.write("READ bc.count" + "\n")
  		tn.write("quit\n")
 		response = tn.read_all()

  		response = response.split('\n')
  		response =filter(lambda x: x.find("port") !=-1, response) 
  		cur_total = sum(map(lambda x: int(x.split()[CNT_INDEX]), response))
  		_, hid,sid,cid,_,_ = response
  		sid = int(sid.split()[CNT_INDEX])
  		cid = int(cid.split()[CNT_INDEX])
  		hid = int(hid.split()[CNT_INDEX])
  		#output = " ".join(map(lambda x: str(x), [total, sid, cid, hid]))
	except:
  		#output = "0 0 0 0"
  		cur_total=0
  
  	if (idle == 0):	
		if (eval('cur_total - prev_total') > 0):
			prev_total = cur_total
			print 'connected... \n'
			
		else:
			idle = 1
			prev_total = cur_total
			print 'SERVER DOWN!!!! \n'
			
			tn = telnetlib.Telnet(HOST, PORT)
			tn.write("WRITE router1/n/proc/rt_SID/rt.set SID13 0" + "\n")
			tn.write("WRITE router1/n/proc/rt_SID/rt.set SID14 0" + "\n")
			
			tn.write("WRITE router1/n/proc/rt_CID/rt.set CID20 0" + "\n")
			tn.write("WRITE router1/n/proc/rt_CID/rt.set CID21 0" + "\n")
			tn.write("WRITE router1/n/proc/rt_CID/rt.set CID22 0" + "\n")
			tn.write("WRITE router1/n/proc/rt_CID/rt.set CID23 0" + "\n")
			tn.write("quit\n")				

		
	else:
		if (eval('cur_total - prev_total') > 0):
			prev_total = cur_total
			idle = 0
			print 'SERVER UP!!!! \n'
			
 			tn = telnetlib.Telnet(HOST, PORT)
			tn.write("WRITE router1/n/proc/rt_SID/rt.set SID13 2" + "\n")
			tn.write("WRITE router1/n/proc/rt_SID/rt.set SID14 2" + "\n")
			
			tn.write("WRITE router1/n/proc/rt_CID/rt.set CID20 2" + "\n")
			tn.write("WRITE router1/n/proc/rt_CID/rt.set CID21 2" + "\n")
			tn.write("WRITE router1/n/proc/rt_CID/rt.set CID22 2" + "\n")
			tn.write("WRITE router1/n/proc/rt_CID/rt.set CID23 2" + "\n")
			tn.write("quit\n")				
			
		else:
			prev_total = cur_total
			print 'Disconnected... \n'
	



