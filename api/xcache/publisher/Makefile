include ../../../xia.mk

.PHONY: all clean announcer

SOURCES=publisher.cc xcache_get_content.cc
OBJECTS=$(SOURCES:.cc=.o)
LIBS=$(XLIB)/libpublisher.so
ANNOUNCER=$(BINDIR)/announce_publisher

ANNOUNCER_SOURCES=announce_publisher.cc

CFLAGS+=-I$(APIDIR)/include -I$(APIDIR)/xcache -I../../common
CFLAGS+=-I$(APIDIR)/xsocket/minini -L$(XLIB)
LDFLAGS+=-L$(XLIB) -lXsocket -ldagaddr -lxcache -lxcachehdr -Wl,-rpath,$(XLIB)

%.o: %.cc
	$(CC) $(CFLAGS) -g -c -fpic $<  -o $@

all: $(OBJECTS)

$(ANNOUNCER): $(ANNOUNCER_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

announcer: $(ANNOUNCER)

clean:
	-rm -f *.o *.so $(OBJECTS) $(ANNOUNCER)
