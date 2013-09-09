include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xcontrold.cc csclient.cc ControlMessage.cc Neighbor.cc
XCONTROLD=$(BINDIR)/xcontrold
LDFLAGS += $(LIBS)

all: $(XCONTROLD)

$(XCONTROLD): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	-rm $(XCONTROLD)

