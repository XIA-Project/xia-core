import getpass
import sys
import telnetlib

HOST = "localhost"
PORT = 7777
CNT_INDEX= 2

output = None
try:
  tn = telnetlib.Telnet(HOST, PORT)
  
  tn.write("READ c.count" + "\n")
  tn.write("quit\n")
  response = tn.read_all()

  response = response.split('\n')
  response =filter(lambda x: x.find("port") !=-1, response) 
  total = sum(map(lambda x: int(x.split()[CNT_INDEX]), response))
  _, hid,sid,cid,_,_ = response
  sid = int(sid.split()[CNT_INDEX])
  cid = int(cid.split()[CNT_INDEX])
  hid = int(hid.split()[CNT_INDEX])
  output = " ".join(map(lambda x: str(x), [total, sid, cid, hid]))
except:
  output = "0 0 0 0"

print output
