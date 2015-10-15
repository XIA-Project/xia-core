include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(LIBS)
SOURCES=xnetjd.cc
NS=$(BINDIR)/xnetjd

all: $(NS)

$(NS): $(SOURCES) $(XINC)/Xsocket.h $(XINC)/xns.h
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)

clean:
	-rm $(NS)
