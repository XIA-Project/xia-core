include ../../xia.mk

.PHONY: all clean

PROTOS         := $(wildcard *.proto)
PROTO_OBJS     := $(PROTOS:.proto=_pb2.py)

%_pb2.py: %.proto
	protoc -I. --python_out=. $<

all: $(PROTO_OBJS)

clean:
	rm -f $(PROTO_OBJS)
