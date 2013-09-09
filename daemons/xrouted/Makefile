include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xrouted.cc csclient.cc XIARouter.cc ControlMessage.cc
XROUTED=$(BINDIR)/xrouted
LDFLAGS += $(LIBS)

all: $(XROUTED)

$(XROUTED): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	-rm $(XROUTED)

