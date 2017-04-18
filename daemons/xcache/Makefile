include ../../xia.mk

VPATH=../common
.PHONY: all clean

XCACHE=$(BINDIR)/xcache

SOURCES=cid.cc xcache.cc cache.cc controller.cc meta.cc store_manager.cc \
	policy_manager.cc XIARouter.cc csclient.cc
OBJS=$(SOURCES:.cc=.o)

LDFLAGS+=$(LIBS) -lprotobuf -lcrypto
CFLAGS+=-I$(APIDIR)/xcache -I ../common -I../../click/include -I$(APIDIR)/xsocket/minini

all:$(XCACHE)

$(XCACHE): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	@-rm -f $(XCACHE)
