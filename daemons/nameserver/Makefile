CC=g++
LD=g++

XIADIR=$(shell cd ../../ && pwd)

APIDIR=$(XIADIR)/api
XINC=$(APIDIR)/include
XLIB=$(APIDIR)/lib
BINDIR=../../bin

CFLAGS=-I$(XINC) -ggdb
LDFLAGS=-lprotobuf -lpthread
SOURCES=ns.cc $(XLIB)/libXsocket.so
NS=$(BINDIR)/xnameservice

all: $(NS)

$(NS): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)	

clean:
	-rm $(NS)
