include ../../../xia.mk

.PHONY: clean

LIBS+=$(XLIB)/libSession.so
LDFLAGS += $(LIBS)
TARGETS=test

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

clean:
	-rm $(TARGETS)
