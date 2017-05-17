include ../../xia.mk

.PHONY: all clean

# directories
STORED_DIR=../stored
VPATH=../common

# target
XCACHE=$(BINDIR)/xcache

# source / obj
SOURCES=cid.cc xcache.cc cache.cc controller.cc meta.cc store_manager.cc stores/mongodb.cc \
	policy_manager.cc XIARouter.cc csclient.cc
DEPS=$(STORED_DIR)/chunk.pb.o $(STORED_DIR)/operation.pb.o $(STORED_DIR)/wrapper.o $(STORED_DIR)/helper.o
OBJS=$(SOURCES:.cc=.o)

# flags
LDFLAGS+=$(LIBS) -lprotobuf -lcrypto
CFLAGS+=-I$(APIDIR)/xcache -I$(VPATH) -I../../click/include -I$(APIDIR)/xsocket/minini -I$(STORED_DIR)

all: $(XCACHE)

$(XCACHE): $(SOURCES)
	$(CC) -o $@ $(CFLAGS) $^ $(DEPS) $(LDFLAGS)

$(STORED_DIR)/%.cc:
	make -C $(STORED_DIR)

clean:
	@-rm -f $(XCACHE)
