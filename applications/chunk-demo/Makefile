include ../../xia.mk

.PHONY: clean

CFLAGS+=-g -I../../../click/include/click
LDFLAGS+=$(LIBS) -lpthread -lprotobuf

TARGETS=chunk-demo-server chunk-demo-client

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
