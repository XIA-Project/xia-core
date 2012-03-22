.PHONY: clean
CC=g++
CFLAGS1=-Wall -Wextra
CFLAGS2=-I../../XIASocket/API
LDFLAGS =  -lprotobuf -lpthread
SOURCES1=routetest.cc csclient.cc XIARouter.cc
SOURCES2=xroute.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a

all: xroute xroute_r0

xroute: $(SOURCES2)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES2) $(LDFLAGS)

xroute_r0: $(SOURCES2)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES2) $(LDFLAGS)

clean:
	rm xroute xroute_r0 
	
