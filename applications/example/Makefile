include ../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)
CFLAGS += -Wno-unused-result

TARGETS= echoclient echoclient-datagram echoclient-simple echoserver time_of_day_client time_of_day_server sidtest xiastreamsocket

all: $(TARGETS)
	make -C dag_manipulation
	#make -C xssl
	make -C speedtest

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

%: %.cc
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

%: %.cpp
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

clean:
	@-rm -f $(TARGETS)
	make -C dag_manipulation clean
	make -C xssl clean
	make -C speedtest clean
