include ../../xia.mk

VPATH=../common

PROTOS		:= $(wildcard *.proto)
PROTO_SRCS	:= $(PROTOS:.proto=.pb.cc)

SUBDIRS=publisher
XCACHE=$(BINDIR)/xcache
CLEAN=$(addsuffix .build, $(SUBDIRS))

.PHONY: all clean $(SUBDIRS)

SOURCES=cid.cc xcache.cc cache.cc controller.cc meta.cc store_manager.cc \
		cache_download.cc \
		publisher_list.cc cid_header.cc ncid_header.cc\
	policy_manager.cc XIARouter.cc csclient.cc ncid_table.cc $(PROTO_SRCS)
OBJS=$(SOURCES:.cc=.o)

LIBS+=-lpublisher
LDFLAGS+=-L$(XLIB) $(LIBS) -lprotobuf -lcrypto
CFLAGS+=-I$(APIDIR)/xcache -I $(XIADIR)/daemons/common -I../../click/include -I$(APIDIR)/xsocket/minini -L$(XLIB)

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
