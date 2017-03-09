include ../../xia.mk

VPATH=../xrouted

.PHONY: all clean

PROTOS         := $(wildcard *.proto) xroute.proto
PROTO_OBJS     := $(PROTOS:.proto=_pb2.py)

%_pb2.py: %.proto
	protoc -I. -I../xrouted --python_out=. $<

all: $(PROTO_OBJS)

clean:
	rm -f $(PROTO_OBJS)
