include ../../../xia.mk
CFLAGS+=-g -I../../../click/include/click -std=c++11
LDFLAGS+=$(LIBS) -lpthread -lprotobuf
all: stage_utils.o ftpServer ftpClient stageManager stageServer xping testXping
	rm stage_utils.o
stage_utils.o:
	$(CC) $(CFLAGS) -c stage_utils.c
ftpServer:
	$(CC) $(CFLAGS) xftp_adv_server.c stage_utils.o -o xftp_adv_server $(LDFLAGS)
ftpClient:
	$(CC) $(CFLAGS) xftp_adv_client.c stage_utils.o -o xftp_adv_client $(LDFLAGS)
stageManager:
	$(CC) $(CFLAGS) stage_manager.c stage_utils.o -o stage_manager $(LDFLAGS)
stageServer:
	$(CC) $(CFLAGS) stage_server.c stage_utils.o -o stage_server $(LDFLAGS)
xping:
	$(CC) $(CFLAGS) xping.c -o xping $(LDFLAGS)
testXping:
	$(CC) $(CFLAGS) testXping.c stage_utils.o -o testXping $(LDFLAGS) 
clean:
	rm -f stage_server stage_manager xftp_adv_server xftp_adv_client xping testXping
