import getpass
import sys
import telnetlib
import time

HOST = "localhost"
PORT = 7000
CNT_INDEX= 2

prev_total = 0

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
  
	if (eval('cur_total - prev_total') > 0):
		prev_total = cur_total
		total = eval('cur_total - prev_total')
		#print 'cumulative_pkts: %s  \n' % cur_total 
	else:
		#print 'SERVER DOWN!!!! \n'
		tn = telnetlib.Telnet(HOST, PORT)
		tn.write("WRITE router1/n/proc/rt_SID/rt.add SID6 1" + "\n")
		tn.write("quit\n")
		break
		
		



