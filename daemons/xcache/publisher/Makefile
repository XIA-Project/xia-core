include ../../../xia.mk

.PHONY: all clean

SOURCES=publisher.cc publisher_cert.cc
OBJECTS=$(SOURCES:.cc=.o)
LIBS=$(XLIB)/libpublisher.so
STATICLIB=$(XLIB)/libpublisher.a
ANNOUNCER=$(BINDIR)/announce_publisher

ANNOUNCER_SOURCES=announce_publisher.cc

CFLAGS+=-I$(APIDIR)/include -I$(APIDIR)/xcache -I../../common
CFLAGS+=-I$(APIDIR)/xsocket/minini -L$(XLIB)
LDFLAGS+=-L$(XLIB) -lXsocket -ldagaddr -lxcache -Wl,-rpath,$(XLIB)

all: $(LIBS) $(ANNOUNCER)

static: $(STATICLIB)

%.o: %.cc
	$(CC) $(CFLAGS) -g -c -fpic $<  -o $@

$(LIBS): $(OBJECTS)
	$(LD) -shared -g -o $@ $(OBJECTS) -lc -ldl $(LDFLAGS)

$(STATICLIB): $(OBJECTS)
	ar rcs $@ $(OBJECTS)

$(ANNOUNCER): $(ANNOUNCER_SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	-rm -f *.o *.so $(LIBS) $(STATICLIB) $(ANNOUNCER)
