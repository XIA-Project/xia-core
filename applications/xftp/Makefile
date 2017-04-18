include ../../xia.mk

.PHONY: clean

LDFLAGS+=$(LIBS) $(XLIB)/libxcache.so -lprotobuf

TARGETS=xftp xftpd

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	@-rm -f $(TARGETS)
