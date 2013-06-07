include ../../xia.mk

.PHONY: clean

LIBS += $(XLIB)/libSession.so
EXTRA_SOURCES = utils.cpp
TARGETS = proxy http_server

all: $(TARGETS)

%: %.cpp
	$(CC) $(CFLAGS) $< $(EXTRA_SOURCES) -o $@ $(LIBS)

clean:
	-rm -f $(TARGETS)
