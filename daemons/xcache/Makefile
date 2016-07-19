include ../../xia.mk

VPATH=../common
.PHONY: all clean

XCACHE=$(BINDIR)/xcache

SOURCES=xcache.cc cache.cc controller.cc logger.cc meta.cc \
	store_manager.cc \
	XIARouter.cc csclient.cc
OBJS=xcache_cache_click.proto $(SOURCES:.cc=.o)

LDFLAGS+=$(LIBS) -lprotobuf -lcrypto
CFLAGS+=-I$(APIDIR)/xcache -I ../common -I../../click/include

all:$(XCACHE)

$(XCACHE): xcache_cache_click.pb.cc $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

xcache_cache_click.pb.cc: xcache_cache_click.proto
	protoc --cpp_out=. xcache_cache_click.proto

clean:
	@-rm *.pb.*
	@-rm $(XCACHE)

#cache.h
#cid.h
#controller.h
#logger.h
#meta.h
#store_manager.h
