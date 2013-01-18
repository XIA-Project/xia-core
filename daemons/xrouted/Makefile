.PHONY: clean
CC=g++
CFLAGS1=-Wall -Wextra
CFLAGS2=-I../../XIASocket/API
LDFLAGS =  -lprotobuf -lpthread
SOURCES=xrouted.cc ../common/csclient.cc ../common/XIARouter.cc ../../XIASocket/API/libXsocket.a
XROUTED=xrouted

all: $(XROUTED)

$(XROUTED): $(SOURCES)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES) $(LDFLAGS)

clean:
	rm $(XROUTED)

