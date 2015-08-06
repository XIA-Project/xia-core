PROTOC=protoc

OBJS=xcache_cmd.pb.o xcache_cache_click.pb.o
CFLAGS+=-fpic

all: $(OBJS)

%.pb.cc: %.proto
	$(PROTOC) $^ --cpp_out=$(XCACHEDIR)/proto
	$(PROTOC) $^ --python_out=$(XCACHEDIR)/proto

%.pb.o: %.pb.cc
	$(CC) $(CFLAGS) -c $^ -o $@

clean:
	rm -f *.o
	rm -f *.h
	rm -f *.cc
	rm -f *.py
