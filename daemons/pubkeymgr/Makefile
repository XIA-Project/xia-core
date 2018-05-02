include ../../xia.mk

.PHONY: all clean

SOURCES=$(wildcard *.cc)
OBJECTS=$(SOURCES:.cc=.o)

PUBLISHER_KEY_MANAGER=$(BINDIR)/pubkeymgr

CFLAGS+=-I$(APIDIR)/include -I$(APIDIR)/xcache -I../../common
CFLAGS+=-I$(APIDIR)/xsocket/minini -L$(XLIB)
LDFLAGS+=-L$(XLIB) -lXsocket -ldagaddr -lxcache -lcrypto -lprotobuf -Wl,-rpath,$(XLIB)

all: $(PUBLISHER_KEY_MANAGER)

$(PUBLISHER_KEY_MANAGER): $(OBJECTS)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

%.o: %.cc
	$(CC) $(CFLAGS) -g -c -fpic $<  -o $@

clean:
	-rm -f $(OBJECTS) $(PUBLISHER_KEY_MANAGER)
