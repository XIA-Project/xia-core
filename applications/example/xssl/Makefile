include ../../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)

TARGETS=test echoserver echoclient

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

clean: 
	-rm $(TARGETS)
