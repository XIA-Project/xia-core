PROTOC=protoc

OBJS=xcache_cmd.pb.o xcache_cache_click.pb.o
CFLAGS+=-fpic

.PHONY: all

LIBXCACHEPROTO=libxcacheproto.a

$(LIBXCACHEPROTO): $(OBJS)
	ar rcs $@ $^

all: $(LIBXCACHEPROTO)

%.pb.cc: %.proto
	$(PROTOC) $^ --cpp_out=$(XCACHEDIR)/proto
	$(PROTOC) $^ --python_out=$(XCACHEDIR)/proto

%.pb.o: %.pb.cc
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -f $(OBJS) $(LIBXCACHEPROTO) *.h *.cc *.py
