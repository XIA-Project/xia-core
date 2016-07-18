VPATH=../../common

LDFLAGS+=-lprotobuf

XCACHE=xcache
XCACHELIBS=../src/libxcached.a ../proto/libxcacheproto.a ../api/libxcache.a

.PHONY: all

COMMONOBJS=csclient.o XIARouter.o

$(XCACHE): $(COMMONOBJS) $(XCACHELIBS) $(COMMON)
	$(CC) $(CFLAGS) -o xcache $(XCACHELIBS) $(COMMONOBJS) $(LDFLAGS)

%.o: %.cc
	$(CC) $(CFLAGS) -o $@ -c $^

all: $(XCACHE)

clean:
	rm -f $(COMMONOBJS) xcache
