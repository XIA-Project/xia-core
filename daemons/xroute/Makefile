.PHONY: clean
CC=g++
CFLAGS1=-Wall -Wextra
CFLAGS2=-I../../XIASocket/API
LDFLAGS =  -lprotobuf -lpthread
SOURCES=xroute.cc ../common/csclient.cc ../common/XIARouter.cc ../../XIASocket/API/libXsocket.a
XROUTE=xrouted

all: $(XROUTE)

$(XROUTE): $(SOURCES)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES) $(LDFLAGS)

clean:
	rm $(XROUTE)

