.PHONY: clean
CC=g++
CFLAGS1=-Wall -Wextra
CFLAGS2=-I../../XIASocket/API
LDFLAGS =  -lprotobuf -lpthread
SOURCES=xroute.cc csclient.cc XIARouter.cc ../../XIASocket/API/libXsocket.a

all: xroute

xroute: $(SOURCES)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES) $(LDFLAGS)

clean:
	rm xroute
	
