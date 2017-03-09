include ../../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)

TARGETS= speedtest-server-stream speedtest-server-dgram speedtest-server-tcpip \
		 speedtest-client-stream speedtest-client-dgram speedtest-client-tcpip

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	@-rm -f $(TARGETS)
