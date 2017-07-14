include ../../xia.mk

VPATH=../common

SUBDIRS=publisher
XCACHE=$(BINDIR)/xcache
CLEAN=$(addsuffix .build, $(SUBDIRS))

.PHONY: all clean $(SUBDIRS)

SOURCES=cid.cc xcache.cc cache.cc controller.cc meta.cc store_manager.cc \
	policy_manager.cc XIARouter.cc csclient.cc
OBJS=$(SOURCES:.cc=.o)

LIBS+=$(XLIB)/libpublisher.so
LDFLAGS+=$(LIBS) -lprotobuf -lcrypto
CFLAGS+=-I$(APIDIR)/xcache -I ../common -I../../click/include -I$(APIDIR)/xsocket/minini

all:$(SUBDIRS) $(XCACHE)
	echo doing all

$(SUBDIRS):
	echo doing make -C in $@
	make -C $@

$(XCACHE): $(SOURCES)
	echo building xcache
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean: $(CLEAN)
	@-rm -f $(XCACHE)

$(CLEAN):
	-make -C $(basename $@) clean
