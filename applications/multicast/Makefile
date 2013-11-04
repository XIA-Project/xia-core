include ../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)
CFLAGS += -std=c++0x

TARGETS= multicast_source multicast_endhost multicast_rp

all: $(TARGETS)
	cp multicast_endhost multicast_endhost_1	
	cp multicast_endhost multicast_endhost_2	
%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

%: %.cpp
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

clean: 
	-rm $(TARGETS)
