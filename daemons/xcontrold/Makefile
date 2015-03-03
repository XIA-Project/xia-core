include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xcontrold.cc csclient.cc ControlMessage.cc XIARouter.cc
XCONTROLD=$(BINDIR)/xcontrold
CFLAGS += -I$(APIDIR)/xsocket/minini
LDFLAGS += $(LIBS)

all: $(XCONTROLD)

$(XCONTROLD): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	-rm $(XCONTROLD)
