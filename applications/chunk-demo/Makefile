include ../../xia.mk

.PHONY: clean

LDFLAGS+=$(LIBS) -lprotobuf 

TARGETS=chunk-demo-server chunk-demo-client ncid-demo-server ncid-demo-client push-demo-server push-demo-client push-demo-proxy

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
