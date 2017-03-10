include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xrouted.cc csclient.cc XIARouter.cc ControlMessage.cc RouterConfig.cc IntraDomainRouter.cc RouteModule.cc Controller.cc xroute.pb.cc

XROUTED=$(BINDIR)/xrouted
LDFLAGS += $(LIBS) -lprotobuf
CFLAGS += -std=c++11 -I$(APIDIR)/xsocket/minini

all: $(XROUTED)

xroute.pb.cc: xroute.proto
	protoc --cpp_out=. $<

$(XROUTED): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	-rm $(XROUTED)
	- rm xroute.pb.*
