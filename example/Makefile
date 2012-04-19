.PHONY: clean

CC=g++
CFLAGS=-I../XIASocket/API
LDFLAGS =  -lprotobuf -lpthread
APPS = tods todc xecho echoclient-d echoclient echoserver xcmpping xcmptraceroute  ccp ccps
LIBFILES = libXsocket.a libXsocket.so _xsocket.so xsocket.py

all: $(LIBFILES) $(APPS)

libXsocket.a:
	ln -s ../XIASocket/API/libXsocket.a .

libXsocket.so:
	ln -s ../XIASocket/API/libXsocket.so .

_xsocket.so:
	ln -s ../XIASocket/API/python/_xsocket.so .

xsocket.py:
	ln -s ../XIASocket/API/python/xsocket.py .

ccps: ccps.c libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< libXsocket.a $(LDFLAGS) 

ccp: ccp.c libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< libXsocket.a $(LDFLAGS) 

xecho: xecho.c  libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< libXsocket.a $(LDFLAGS) 

todc: todc.c  libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< libXsocket.a $(LDFLAGS) 

tods: tods.c  libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< libXsocket.a $(LDFLAGS) 

echoclient-d: echoclient-d.c  libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< libXsocket.a $(LDFLAGS) 

echoclient: echoclient.c  libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< libXsocket.a $(LDFLAGS) 

echoserver: echoserver.c  libXsocket.a
	$(CC) -Wall -Wextra $(CFLAGS) -o $@ $< libXsocket.a $(LDFLAGS) 

xcmpping: xcmpping.c libXsocket.a ./XIAResolver/XgetDAGbyname.c
	gcc $(CFLAGS) -o $@ $< libXsocket.a  ./XIAResolver/XgetDAGbyname.c $(LDFLAGS) 

xcmptraceroute: xcmptraceroute.c libXsocket.a ./XIAResolver/XgetDAGbyname.c
	gcc $(CFLAGS) -o $@ $< libXsocket.a ./XIAResolver/XgetDAGbyname.c $(LDFLAGS) 

clean:
	rm -f $(APPS)
	rm -f $(LIBFILES)


