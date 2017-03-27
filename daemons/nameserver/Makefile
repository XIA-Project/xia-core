include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(LIBS)
SOURCES=ns.cc
NS=$(BINDIR)/xnameservice

all: $(NS)

$(NS): $(SOURCES) $(XINC)/Xsocket.h $(XINC)/xns.h
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)

clean:
	@-rm -f $(NS)
