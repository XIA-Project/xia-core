include ../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)
#CFLAGS += -std=c++0x

TARGETS=lb_client lb_server 

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

%: %.cpp
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

clean: 
	-rm $(TARGETS)
