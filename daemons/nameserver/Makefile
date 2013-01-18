CC=g++
LD=g++

APIDIR=../../api
XINC=$(APIDIR)/include
XLIB=$(APIDIR)/lib
BINDIR=../../bin

CFLAGS=-I$(APIDIR)/xsocket -ggdb
LDFLAGS=-lprotobuf -lpthread
SOURCES=ns.cc $(XLIB)/libXsocket.so
NS=$(BINDIR)/xnameservice

all: $(NS)

$(NS): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)	

clean:
	rm $(NS)
	
	
