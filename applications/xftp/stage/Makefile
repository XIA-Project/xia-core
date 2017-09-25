include ../../../xia.mk
CFLAGS+=-g -I../../../click/include/click -std=c++11
LDFLAGS+=$(LIBS) -lpthread -lprotobuf
all: stage_utils.o getWifiInfo ftpServer ftpClient mulServer mulClient stageManager stage_manager_multihoming stage_manager_soft stageServer stage_server_multihoming stage_server_soft xping testXping switchAP AP multihoming_client multihoming_server disconnection predic_adv_client predic_server
	rm stage_utils.o
stage_utils.o:
	$(CC) $(CFLAGS) -c stage_utils.c
getWifiInfo:
	$(CC) $(CFLAGS) getWifiInfo.c stage_utils.o -o getWifiInfo $(LDFLAGS)
predic_adv_client:
	$(CC) $(CFLAGS) predic_adv_client.c stage_utils.o -o predic_adv_client $(LDFLAGS)
predic_server:
	$(CC) $(CFLAGS) predic_server.c stage_utils.o -o predic_server $(LDFLAGS)
ftpServer:
	$(CC) $(CFLAGS) xftp_adv_server.c stage_utils.o -o xftp_adv_server $(LDFLAGS)
ftpClient:
	$(CC) $(CFLAGS) xftp_adv_client.c stage_utils.o -o xftp_adv_client $(LDFLAGS)
mulServer:
	$(CC) $(CFLAGS) mulServer.c stage_utils.o -o mulServer $(LDFLAGS)
mulClient:
	$(CC) $(CFLAGS) mulClient.c stage_utils.o -o mulClient $(LDFLAGS)
stageManager:
	$(CC) $(CFLAGS) stage_manager.c stage_utils.o -o stage_manager $(LDFLAGS)
stage_manager_multihoming:
	$(CC) $(CFLAGS) stage_manager_multihoming.c stage_utils.o -o stage_manager_multihoming $(LDFLAGS)
stage_manager_soft:
	$(CC) $(CFLAGS) stage_manager_soft.c stage_utils.o -o stage_manager_soft $(LDFLAGS)
stageServer:
	$(CC) $(CFLAGS) stage_server.c stage_utils.o -o stage_server $(LDFLAGS)
stage_server_multihoming:
	$(CC) $(CFLAGS) stage_server_multihoming.c stage_utils.o -o stage_server_multihoming $(LDFLAGS)
stage_server_soft:
	$(CC) $(CFLAGS) stage_server_soft.c stage_utils.o -o stage_server_soft $(LDFLAGS)
xping:
	$(CC) $(CFLAGS) xping.c -o xping $(LDFLAGS)
testXping:
	$(CC) $(CFLAGS) testXping.c stage_utils.o -o testXping $(LDFLAGS) 
switchAP:
	$(CC) $(CFLAGS) switchAP.c stage_utils.o -o switchAP $(LDFLAGS)
AP:
	$(CC) $(CFLAGS) AP.c stage_utils.o -o AP $(LDFLAGS)
multihoming_client:
	$(CC) $(CFLAGS) multihoming_client.c stage_utils.o -o multihoming_client $(LDFLAGS)
multihoming_server:
	$(CC) $(CFLAGS) multihoming_server.c -o multihoming_server $(LDFLAGS)
disconnection:
	$(CC) $(CFLAGS) disconnection.c stage_utils.o -o disconnection $(LDFLAGS)
clean:
	rm -f getWifiInfo stage_server stage_manager xftp_adv_server xftp_adv_client xping testXping switchAP AP multihoming_client multihoming_server disconnection stage_manager_multihoming stage_server_multihoming mulServer mulClient stage_manager_soft stage_server_soft predic_adv_client predic_server stage_utils.o
