.PHONY: clean
CC=g++
CFLAGS1=-Wall -Wextra
CFLAGS2=-I../../XIASocket/API
LDFLAGS =  -lprotobuf -lpthread
SOURCES_R0=xroute.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a
SOURCES_R1=xroute_r1.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a

all: xroute xroute_r0 xroute_r1

xroute: $(SOURCES_R0)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES_R0) $(LDFLAGS)

xroute_r0: $(SOURCES_R0)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES_R0) $(LDFLAGS)
		
xroute_r1: $(SOURCES_R1)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES_R1) $(LDFLAGS)	

clean:
	rm xroute xroute_r0 xroute_r1
	
