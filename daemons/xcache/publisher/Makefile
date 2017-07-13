include ../../../xia.mk

.PHONY: all clean

SOURCES=publisher.cc
OBJECTS=$(SOURCES:.cc=.o)
LIBS=$(XLIB)/libpublisher.so
STATICLIB=$(XLIB)/libpublisher.a

CFLAGS+=-I$(APIDIR)/include

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
