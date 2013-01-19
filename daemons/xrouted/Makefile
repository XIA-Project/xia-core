.PHONY: clean

APIDIR=../../api
XINC=$(APIDIR)/include
XLIB=$(APIDIR)/lib
BINDIR=../../bin

CC=g++
CFLAGS1=-Wall -Wextra
CFLAGS2=-I$(XINC)
LDFLAGS =  -lprotobuf -lpthread
SOURCES=xrouted.cc ../common/csclient.cc ../common/XIARouter.cc $(XLIB)/libXsocket.so
XROUTED=$(BINDIR)/xrouted

all: $(XROUTED)

$(XROUTED): $(SOURCES)
	$(CC) -o $@ $(CFLAGS2) $(SOURCES) $(LDFLAGS)

clean:
	-rm $(XROUTED)

