include ../../../xia.mk

.PHONY: all clean

SOURCES=publisher.cc xcache_get_content.cc
OBJECTS=$(SOURCES:.cc=.o)
LIBS=$(XLIB)/libpublisher.so
ANNOUNCER=$(BINDIR)/announce_publisher

ANNOUNCER_SOURCES=announce_publisher.cc

CFLAGS+=-I$(APIDIR)/include -I$(APIDIR)/xcache -I../../common
CFLAGS+=-I$(APIDIR)/xsocket/minini -L$(XLIB)
LDFLAGS+=-L$(XLIB) -lXsocket -ldagaddr -lxcache -lxcachehdr -Wl,-rpath,$(XLIB)

all: $(LIBS) $(ANNOUNCER)


%.o: %.cc
	$(CC) $(CFLAGS) -g -c -fpic $<  -o $@

$(LIBS): $(OBJECTS)
	$(LD) -shared -g -o $@ $(OBJECTS) -lc -ldl $(LDFLAGS)

$(STATICLIB): $(OBJECTS)
	ar rcs $@ $(OBJECTS)

$(ANNOUNCER): $(ANNOUNCER_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lpublisher

clean:
	-rm -f *.o *.so $(LIBS) $(STATICLIB) $(ANNOUNCER)
