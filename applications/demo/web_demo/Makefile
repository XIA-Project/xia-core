include ../../../xia.mk

.PHONY: clean

LDFLAGS += $(XLIB)/libXsocket.so $(XLIB)/libdagaddr.so
APPS = vs

all: $(APPS)

vs: video_server.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(APPS)
