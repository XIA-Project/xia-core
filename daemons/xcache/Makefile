include ../../xia.mk

VPATH=../common

PROTOS		:= $(wildcard *.proto)
PROTO_SRCS	:= $(PROTOS:.proto=.pb.cc)

SUBDIRS=publisher
XCACHE=$(BINDIR)/xcache
CLEAN=$(addsuffix .build, $(SUBDIRS))

.PHONY: all clean $(SUBDIRS)

SOURCES=cid.cc xcache.cc cache.cc controller.cc meta.cc store_manager.cc \
		publisher_list.cc \
	policy_manager.cc XIARouter.cc csclient.cc ncid_table.cc $(PROTO_SRCS)
OBJS=$(SOURCES:.cc=.o)

LIBS+=$(XLIB)/libpublisher.so
LDFLAGS+=$(LIBS) -lprotobuf -lcrypto
CFLAGS+=-I$(APIDIR)/xcache -I ../common -I../../click/include -I$(APIDIR)/xsocket/minini

%.pb.cc: %.proto
	protoc -I. --cpp_out=. $<

all:$(PROTO_SRCS) $(SUBDIRS) $(XCACHE)


$(SUBDIRS):
	make -C $@

$(XCACHE): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean: $(CLEAN)
	@-rm -f $(XCACHE)
	rm -f $(PROTO_SRCS) *.pb.cc *.pb.h

$(CLEAN):
	-make -C $(basename $@) clean
