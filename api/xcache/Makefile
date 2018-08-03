include ../../xia.mk

SUBDIRS=headers publisher
CLEANDIRS=$(addsuffix .build, $(SUBDIRS))

PROTOC=protoc
CFLAGS+=-c -fpic -I$(APIDIR)/xcache
LDFLAGS+=-lprotobuf $(XLIB)/libXsocket.so $(XLIB)/libxcachehdr.so

PROTOS		:= $(wildcard *.proto)
PROTO_SRCS	:= $(PROTOS:.proto=.pb.cc)
PROTO_OBJS	:= $(PROTO_SRCS:.cc=.o)

PUBLISHER_SRCS	:= publisher/publisher.cc publisher/xcache_get_content.cc
PUBLISHER_OBJS := $(PUBLISHER_SRCS:.cc=.o)

.PHONY: all clean $(SUBDIRS) $(CLEANDIRS) announce_publisher

SOURCES=XcacheApis.c xcache_sock.c
OBJS=$(SOURCES:.c=.o)

LIB=$(XLIB)/libxcache.so

all: $(SUBDIRS) $(LIB) announce_publisher

%.pb.cc: %.proto
	$(PROTOC) --cpp_out=. $<

%.pb.o: %.pb.cc
	$(CC) $(CFLAGS) -Wno-unused-parameter $< -o $@

%.o: %.c $(INC)/xcache.h
	$(CC) $(CFLAGS) $< -o $@

$(LIB): $(PROTO_OBJS) $(OBJS) $(PUBLISHER_OBJS)
	$(LD) -fPIC --shared -o $@ $^ $(LDFLAGS)

$(SUBDIRS):
	make -C $@

announce_publisher:
	make -C publisher announcer

$(CLEANDIRS):
	-make -C $(basename $@) clean

clean: $(CLEANDIRS)
	rm -f $(OBJS)
	rm -f *.pb.*
	rm -f $(LIB)
