include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(XLIB)/libXsocket.so
SOURCES=ns.cc
NS=$(BINDIR)/xnameservice

all: $(NS)

$(NS): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)	

clean:
	-rm $(NS)
