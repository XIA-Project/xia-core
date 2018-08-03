include ../../xia.mk
VPATH=../common

.PHONY: all clean

ROUTER_OBJS=xrouted.o Router.o
CONTROL_OBJS=xcontrold.o Controller.o
HOST_OBJS=xhostd.o Host.o
COMMON_OBJS=xroute.pb.o RouteModule.o RouterConfig.o csclient.o XIARouter.o Settings.o

XROUTED=$(BINDIR)/xrouted
XCONTROLD=$(BINDIR)/xcontrold
XHOSTD=$(BINDIR)/xhostd
LDFLAGS += $(LIBS) -lprotobuf
CFLAGS += -std=c++11 -I$(APIDIR)/xsocket/minini

all: $(XROUTED) $(XCONTROLD) $(XHOSTD)

xroute.pb.cc xroute.pb.h: xroute.proto
	protoc --cpp_out=. $<

$(XROUTED): $(COMMON_OBJS) $(ROUTER_OBJS)
	$(CC) -o $@ $(CFLAGS) -Wno-unused-parameter $^ $(LDFLAGS)

$(XCONTROLD): $(COMMON_OBJS) $(CONTROL_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(XHOSTD): $(COMMON_OBJS) $(HOST_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.cc
	$(CC) -c $(CFLAGS) $<  -o $@

clean:
	@-rm -f $(XROUTED)
	@-rm -f *.o
	@-rm -f xroute.pb.*

