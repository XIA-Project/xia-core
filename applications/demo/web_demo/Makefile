include ../../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)
APPS = vs

all: $(APPS)

vs: video_server.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(APPS)
