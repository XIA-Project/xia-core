include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(LIBS)
SOURCES=rvd.cc
NS=$(BINDIR)/xrendezvous

all: $(NS)

$(NS): $(SOURCES) $(XINC)/Xsocket.h $(XINC)/xns.h
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)

clean:
	@-rm -f $(NS)
