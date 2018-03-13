include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xrouted.cc csclient.cc XIARouter.cc xroute.pb.cc
XROUTED=$(BINDIR)/xrouted
LDFLAGS += $(LIBS) -lprotobuf

all: $(XROUTED)

xroute.pb.cc: xroute.proto
	protoc --cpp_out=. $<

$(XROUTED): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	@-rm -f $(XROUTED)
	@-rm -f xroute.pb.*

