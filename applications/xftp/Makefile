include ../../xia.mk

.PHONY: clean

CFLAGS+=-g -I../../../click/include/click
LDFLAGS+=$(LIBS) -lpthread -lprotobuf

TARGETS=xftp_basic_server xftp_basic_client

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

clean:
	rm -f $(TARGETS)
