include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xrouted.cc csclient.cc XIARouter.cc
XROUTED=$(BINDIR)/xrouted
LDFLAGS += $(XLIB)/libXsocket.so

all: $(XROUTED)

$(XROUTED): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	-rm $(XROUTED)

