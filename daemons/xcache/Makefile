include ../../xia.mk

.PHONY: all clean

# directories
STORED_DIR=../stored
PROTO_DIR=$(STORED_DIR)/proto
VPATH=../common

# target
XCACHE=$(BINDIR)/xcache

# source / obj
SOURCES=cid.cc xcache.cc cache.cc controller.cc meta.cc store_manager.cc \
	policy_manager.cc XIARouter.cc csclient.cc
DEPS=$(PROTO_DIR)/chunk.pb.cc $(PROTO_DIR)/operation.pb.cc $(STORED_DIR)/wrapper.cc $(STORED_DIR)/helper.cc
OBJS=$(SOURCES:.cc=.o)

# flags
LDFLAGS+=$(LIBS) -lprotobuf -lcrypto
CFLAGS+=-I$(APIDIR)/xcache -I$(VPATH) -I../../click/include -I$(APIDIR)/xsocket/minini -I$(PROTO_DIR) -I$(STORED_DIR)

all: protobuf $(XCACHE)

protobuf:
	$(MAKE) -C $(PROTO_DIR)

$(XCACHE): $(SOURCES) $(DEPS)
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS)

clean:
	@-rm -f $(XCACHE)
