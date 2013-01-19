APIDIR=../../api
BINDIR=../../bin
CFLAGS=-Wall -I$(APIDIR)/include
LDFLAGS=$(APIDIR)/lib/libXsocket.so $(APIDIR)/lib/libXIAResolver.so -lpthread

all: xping
	mv xping $(BINDIR)/

clean: 
	rm $(BINDIR)/xping