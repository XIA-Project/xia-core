include ../../xia.mk

.PHONY: clean

LIBS += $(XLIB)/libSession.so
EXTRA_SOURCES = utils.cpp http.cpp
TARGETS = proxy http_server pagespeed

all: $(TARGETS)

%: %.cpp $(EXTRA_SOURCES)
	$(CC) $(CFLAGS) $< $(EXTRA_SOURCES) -o $@ $(LIBS)

clean:
	-rm -f $(TARGETS)
