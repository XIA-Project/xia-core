include ../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)
CFLAGS += -Wno-unused-result

TARGETS= push_xftp_recv_client push_xftp_recv_server push_xftp_client push_xftp_server xftp_basic_client xftp_basic_server chunk_copy_client chunk_copy_server echoclient echoclient-datagram \
	echoclient-simple echoserver time_of_day_client time_of_day_server

all: $(TARGETS)
	make -C dag_manipulation
	make -C xssl

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

%: %.cpp
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

clean: 
	-rm $(TARGETS)
	make -C dag_manipulation clean
	make -C xssl clean
