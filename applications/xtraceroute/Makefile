XIADIR=$(shell cd ../../ && pwd)

APIDIR=$(XIADIR)/api
BINDIR=../../bin
CFLAGS=-Wall -I$(APIDIR)/include
LDFLAGS=$(APIDIR)/lib/libXsocket.so $(APIDIR)/lib/libXIAResolver.so -lpthread

all: xtraceroute
	mv xtraceroute $(BINDIR)/

clean: 
	rm $(BINDIR)/xtraceroute