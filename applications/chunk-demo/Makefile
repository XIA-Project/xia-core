include ../../xia.mk

.PHONY: clean

LDFLAGS+=$(LIBS) -lprotobuf 
CFLAGS+=-I$(APIDIR)/xcache/

TARGETS=chunk-demo-server chunk-demo-client ncid-demo-server ncid-demo-client push-demo-server push-demo-client push-demo-proxy fetch-demo-client make-random-chunk icid-demo-client

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
