include ../../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)

TARGETS=echoserver echoclient

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

clean:
	@-rm -f $(TARGETS)
