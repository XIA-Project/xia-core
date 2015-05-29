#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Prefetch Client"

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

char prefetch_client_name[] = "www_s.client.prefetch.aaa.xia";
char prefetch_profile_name[] = "www_s.profile.prefetch.aaa.xia";

int netMonSock; // TODO: monitor connectivity and handoff
int prefetchClientSock;
int prefetchProfileSock;

// receives CID updates from xftp_client and forward to prefetch_profile
void *metaRecvCmd (void *socketid) {

	char command[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];
	int sock = *((int*)socketid);
	int n;

	char fin[512];
	char fout[512];
	int start, end;

	while (1) {
		memset(command, '\0', strlen(command));
		memset(reply, '\0', strlen(reply));

		if ((n = Xrecv(sock, command, RECV_BUF_SIZE, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		// printf("%d\n", n);
		/*
		if (strncmp(command, "Hello from context client", 25) == 0) {
			say("Received hello from context client\n");
			char* hello = "Hello from prefetch client";
			sendCmd(prefetchProfileSock, hello);
		}
		*/
		// TODO
		if (strncmp(command, "get", 3) == 0) {
			say("Received CID request from context client\n");
			printf("%s\n", command);
			sscanf(command, "get %s %d %d", fin, &start, &end);
			//printf("get %s %d %d\n", fin, start, end);
			if (start == 0) {
				sendCmd(prefetchProfileSock, command);
			}
		}
	}

// TODO: handoff msg reception and send to profile
/*
	int selRetVal; 
  struct timeval tv; 
  fd_set socks; 
	char *buffer = getRegisterHostMessage();
	sockaddr_x pseudo_gw_router_dag; // dag for host_register_message (broadcast message), but only the gw router will accept it
	Graph gw = Node() * Node(BHID) * Node(SID_XROUTE);
	gw.fill_sockaddr(&pseudo_gw_router_dag);
	// ends; and how long the total time it takes for this function
	int msg_len = strlen(buffer);
	Xsendto(sockfd, buffer, msg_len, 0, (sockaddr*)&pseudo_gw_router_dag, sizeof(pseudo_gw_router_dag));
	
	char recv_buf[XHCP_MAX_PACKET_SIZE]; 	
	while(1) {
		FD_ZERO(&socks);
		FD_SET(sockfd_ctx, &socks);
		FD_SET(sockfd_cid, &socks);		
		tv.tv_sec = 0;
		tv.tv_usec = 1000000; // every 1 sec, check if any received packets
		
		selRetVal = select(max(sockfd_ctx, sockfd_cid)+1, &socks, NULL, NULL, &tv);
		if (selRetVal > 0) {
			socklen_t dlen = sizeof(sockaddr_x);
			int n = Xrecvfrom(sockfd, recv_buf, XHCP_MAX_PACKET_SIZE, 0, (struct sockaddr*)&pseudo_gw_router_dag, &dlen);
			if (!strncmp(buffer, recv_buf, msg_len)) {
				syslog(LOG_INFO, "Received registration Ack from router after %d retries.", 40 - retries);
				break;
			}
		}
	}
	Xclose(sockfd_ctx);
	Xclose(sockfd_cid);
*/
	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

int main() {
	prefetchProfileSock = initializeClient(prefetch_profile_name, myAD, myHID, prefetchProfileAD, prefetchProfileHID);
	prefetchClientSock = registerStreamReceiver(prefetch_client_name, myAD, myHID, my4ID);
	blockListener((void *)&prefetchClientSock, metaRecvCmd);
}

/*
	while(1) {
		char *ssid = execSystem("iwgetid -r");
		std::cerr<<ssid<<std::endl;
		usleep(1000000);
	}

	*/