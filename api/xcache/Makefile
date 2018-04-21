include ../../xia.mk

SUBDIRS=publisher
PROTOC=protoc
CFLAGS+=-c -fpic
LDFLAGS+=-lprotobuf $(XLIB)/libXsocket.so

PROTOS		:= $(wildcard *.proto)
PROTO_SRCS	:= $(PROTOS:.proto=.pb.cc)
PROTO_OBJS=$(PROTO_SRCS:.cc=.o)

.PHONY: all clean $(SUBDIRS)

SOURCES=XcacheApis.c xcache_sock.c
OBJS=$(SOURCES:.c=.o)

LIB=$(XLIB)/libxcache.so

all: $(LIB) $(SUBDIRS)


%.pb.cc: %.proto
	$(PROTOC) --cpp_out=. $<

%.pb.o:  %.pb.cc
	$(CC) $(CFLAGS) $< -o $@

%.o: %.c $(INC)/xcache.h
	$(CC) $(CFLAGS) $< -o $@

$(LIB): $(PROTO_OBJS) $(OBJS)
	$(LD) -fPIC --shared -o $@ $(OBJS) $(PROTO_OBJS) $(LDFLAGS)

$(SUBDIRS):
	make -C $@

clean:
	rm -f $(OBJS)
	rm -f *.pb.*
	rm -f $(LIB)
	-make -C $(SUBDIRS) clean
