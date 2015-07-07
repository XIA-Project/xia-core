CC=protoc

OBJS=xcache_cmd.pb.o

all: $(OBJS)

%.pb.cc: %.proto
	$(CC) $^ --cpp_out=$(XCACHEDIR)/proto
	$(CC) $^ --python_out=$(XCACHEDIR)/proto

%.pb.o: %.pb.cc
	g++ -c $^ -o $@

clean:
	rm -f *.o
	rm -f *.h
	rm -f *.cc
	rm -f *.py
