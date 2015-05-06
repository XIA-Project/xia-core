include ../../xia.mk

.PHONY: clean

LDFLAGS += $(LIBS)
#CFLAGS += -std=c++0x

TARGETS= push_xftp_recv_client push_xftp_recv_server push_xftp_client push_xftp_server xftp_basic_client xftp_basic_server chunk_copy_client chunk_copy_server echoclient echoclient-datagram \
	echoclient-simple echoserver time_of_day_client time_of_day_server \
	xftp_adv_client prefetch_client prefetch_profile prefetch_pred prefetch_exec xftp_adv_server \
	firehose drink

all: $(TARGETS)
	make -C dag_manipulation
	make -C xssl

%: %.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

%: %.cpp
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) -lpthread

xftp_adv_client: xftp_adv_client.c prefetch_utils.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

prefetch_client: prefetch_client.c prefetch_utils.c  
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

prefetch_profile: prefetch_profile.c prefetch_utils.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

prefetch_pred: prefetch_pred.c prefetch_utils.c  
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

prefetch_exec: prefetch_exec.c prefetch_utils.c  
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

xftp_adv_server: xftp_adv_server.c prefetch_utils.c
	$(CC) -o $@ $(CFLAGS) $^ $(LDFLAGS) -lpthread

clean: 
	-rm $(TARGETS)
	make -C dag_manipulation clean
	make -C xssl clean
