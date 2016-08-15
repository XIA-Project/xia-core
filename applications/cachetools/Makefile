include ../../xia.mk

#VPATH=../../daemons/xcache

.PHONY: clean

LDFLAGS+=$(LIBS) -lprotobuf -lcrypto

TARGETS=cacheload cachepurge

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
