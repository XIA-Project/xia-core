#!/usr/bin/python
import xsocket 
from xia_address import * 
import random
import sys
from xsocket import *
from ctypes import *

class Stock:
    def __init__(self, name):
        self.name = name
	self.begin = random.uniform(1,100)
	self.price = self.begin
	self.delta = 0
    def update(self):
        rate_change = random.uniform(0.8, 1.2) 
    	self.delta = self.begin - self.begin * rate_change
    	self.price = self.begin + self.delta

def update_stock(stock):
    for s in stock:
        s.update()

def update_stockfeed(stock):
    update_stock(stock)
    #updata = "<span class=\"clstsu\">%s %.2f   <img class=\"clstimg\" src=\"uptick.gif\" WIDTH=\"18\" HEIGHT=\"8\">+%.2f</span> "
    #downdata = "<span class=\"clstsu\">%s %.2f   <img class=\"clstimg\" src=\"dwntick.gif\" WIDTH=\"18\" HEIGHT=\"8\">%.2f</span> "
    updata =  "<span class=\"clstsu\">%s %5.2f %+6.2f</span> "
    downdata= "<span class=\"clstsd\">%s %5.2f %+6.2f</span> "
    stable =  "<span class=\"clstst\">%s %5.2f %7.2f</span> "
	
    feed = ""
    for i in range(len(stock)):
    	data  = updata 
	if (stock[i].delta<0):
	    data =  downdata
	if (stock[i].delta ==0):
	    data = stable
	#print stock[i].delta
	if (len(feed) >1100):	
	    break
    	feed =feed+ (data % (stock[i].name, stock[i].price, stock[i].delta))

    return feed


stock_name= ["ADA",  "BQB", "CKK", "GPR", "HER", "IAK", "KOY", "LUR", "XIA", "YRY" ]
stock = map(lambda name: Stock(name), stock_name)
set_conf("xsockconf_python.ini","stock_service_replicate_cmu.py")
print_conf()



try:
        sock=Xsocket(XSOCK_DGRAM)
        
        if (sock<0):
        	print "error opening socket"
        	exit(-1)
        
        # Make the sDAG (the one the server listens on)
        dag = "RE %s %s %s" % (AD_CMU, HID2, SID_STOCK_REPLICATE)
        
        # Bind to the DAG
        ret= Xbind(sock,dag);
        print "Stock_servce: listening on %s" % dag
        print "Stock_servce: bind returns %d socket %d" % (ret, sock)
        

    	while(True):	
  		replyto =  ''
		data = ''

        	(data, replyto) = Xrecvfrom(sock, 2000, 0)
 
        		
        	stock_feed = update_stockfeed(stock)
		http_header = "HTTP/1.1 200 OK\nDate: Sat, 08 Jan 2011 22:25:07 GMT\nServer: Apache/2.2.17 (Unix)\nAccess-Control-Allow-Origin: *\nCache-Control: no-cache\nConnection: close\nContent-Type: text/plain\nLast-Modified: 100\n\n"
        	
		response = http_header+ stock_feed
		#print "Stock_servce: response to: %s" % replyto
		#print "Stock_servce: response: %s" % data
		print "Stock_servce: response len %d" % len(response)
			
		Xsendto(sock, response, len(response), 0, replyto, len(replyto)+1)	
    
        		      		
except (KeyboardInterrupt, SystemExit), e:
	sys.exit()
        Xclose(sock)



