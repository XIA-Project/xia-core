include ../../xia.mk
VPATH=../common

.PHONY: all clean

SOURCES=xrouted.cc csclient.cc XIARouter.cc ControlMessage.cc RouterConfig.cc IntraDomainRouter.cc RouteModule.cc Controller.cc
XROUTED=$(BINDIR)/xrouted
LDFLAGS += $(LIBS)
CFLAGS += -std=c++11 -I$(APIDIR)/xsocket/minini

all: $(XROUTED)

$(XROUTED): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	-rm $(XROUTED)
