include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(LIBS)
SOURCES=sciongateway.cc
NS=$(BINDIR)/xgateway

all: $(NS)

$(NS): $(SOURCES) $(XINC)/Xsocket.h $(XINC)/xns.h
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)	

clean:
	-rm $(NS)
