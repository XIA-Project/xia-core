include ../../xia.mk

.PHONY: clean

CFLAGS+=-g -I../../../click/include/click -I$(XCACHEINC)
LDFLAGS+=$(LIBS) -lpthread -lprotobuf

TARGETS=xftp xftpd

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGETS)
