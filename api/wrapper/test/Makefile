include ../../../xia.mk

.PHONY: all test

PRELOAD=$(XLIB)/xwrap.so $(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so

TARGETS=xwtest nametest

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(XLIB)/libdagaddr.so

test: $(TARGETS)
	@$(BINDIR)/xwrap ./nametest
	@#LD_PRELOAD="$(LIBS)"; ./xwtest

clean:
	@-rm -f $(TARGETS)
