include ../../xia.mk

.PHONY: clean

LDFLAGS+=$(LIBS) -lprotobuf 

TARGETS=chunk-demo-server chunk-demo-client

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
