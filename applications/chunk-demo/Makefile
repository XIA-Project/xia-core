include ../../xia.mk

.PHONY: clean

LDFLAGS+=$(LIBS) -lprotobuf 

TARGETS=chunk-demo-server chunk-demo-client ncid-demo-server ncid-demo-client

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
