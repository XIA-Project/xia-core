include ../../../xia.mk

.PHONY: all clean

SOURCES=publisher.cc publisher_cert.cc
OBJECTS=$(SOURCES:.cc=.o)
LIBS=$(XLIB)/libpublisher.so
STATICLIB=$(XLIB)/libpublisher.a

CFLAGS+=-I$(APIDIR)/include -I$(APIDIR)/xcache -I../../common
CFLAGS+=-I$(APIDIR)/xsocket/minini -L$(XLIB)
LDFLAGS+=-L$(XLIB) -lXsocket -ldagaddr -lxcache -Wl,-rpath,$(XLIB)

all: $(LIBS)

static: $(STATICLIB)

%.o: %.cc
	$(CC) $(CFLAGS) -g -c -fpic $<  -o $@

$(LIBS): $(OBJECTS)
	$(LD) -shared -g -o $@ $(OBJECTS) -lc -ldl $(LDFLAGS)

$(STATICLIB): $(OBJECTS)
	ar rcs $@ $(OBJECTS)

clean:
	-rm -f *.o *.so $(LIBS) $(STATICLIB)
