include ../../xia.mk

.PHONY: all clean

LDFLAGS += $(LIBS)
SOURCES=sciongateway.cc
GW=$(BINDIR)/xgatewayd

all: $(GW)

$(GW): $(SOURCES) $(XINC)/Xsocket.h
	$(CC) -o $@ $(CFLAGS) $(SOURCES) $(LDFLAGS)	

clean:
	-rm $(GW)
