include ../../xia.mk
VPATH=../common

.PHONY: all clean

ROUTER_OBJS=xrouted.o Router.o
CONTROL_OBJS=controld.o Controller.o
COMMON_OBJS=xroute.pb.o RouteModule.o RouterConfig.o csclient.o XIARouter.o

XROUTED=$(BINDIR)/xrouted
CONTROLD=$(BINDIR)/controld
LDFLAGS += $(LIBS) -lprotobuf
CFLAGS += -std=c++11 -I$(APIDIR)/xsocket/minini

all: $(XROUTED) $(CONTROLD)

xroute.pb.cc xroute.pb.h: xroute.proto
	protoc --cpp_out=. $<

$(XROUTED): $(COMMON_OBJS) $(ROUTER_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(CONTROLD): $(COMMON_OBJS) $(CONTROL_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.cc
	$(CC) -c $(CFLAGS) $<  -o $@

clean:
	@-rm -f $(XROUTED)
	@-rm -f $(OBJS)
	@-rm -f xroute.pb.*
