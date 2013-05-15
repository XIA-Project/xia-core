include ../../../xia.mk

.PHONY: clean

LIBS += $(XLIB)/libSession.so
#LDFLAGS += $(LIBS)
TARGETS = client server #service

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LIBS)

clean:
	-rm -f $(TARGETS)
