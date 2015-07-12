include ../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)
#CFLAGS += -std=c++0x

TARGETS= push_xftp_recv_client push_xftp_recv_server push_xftp_client push_xftp_server xftp_basic_client xftp_basic_server chunk_copy_client chunk_copy_server echoclient echoclient-datagram \
	echoclient-simple echoserver time_of_day_client time_of_day_server \
	echoclient-mobility xftp_adv_client prefetch_manager prefetch_server xftp_adv_server 

all: $(TARGETS)
	make -C dag_manipulation
	make -C xssl

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

%: %.cpp
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

xftp_adv_client: xftp_adv_client.c prefetch_utils.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

prefetch_manager: prefetch_manager.c prefetch_utils.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

prefetch_server: prefetch_server.c prefetch_utils.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

xftp_adv_server: xftp_adv_server.c prefetch_utils.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

clean: 
	-rm $(TARGETS)
	make -C dag_manipulation clean
	make -C xssl clean
