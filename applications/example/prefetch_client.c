/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**        http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*simple interactive echo client using Xsockets */

#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Prefetch Client"

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

char* prefetch_profile_name = "www_s.profile.prefetch.aaa.xia";

// TODO: handoff msg reception
// TODO: clean up
char* prefetch_ctx_name = "www_s.prefetch_context_listener.aaa.xia";
//char* prefetch_pred_name = "www_s.prefetch_prediction_listener.aaa.xia";

int sockfd_ctx;
int sockfd_cid;
int sockfd_pred;


// prefetch_client receives context updates and send predicted CID_s to prefetch_server
// handle the CID update first and location/AP later
void *ctxRecvCmd (void *socketid) {

	char command[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	int n;

	char fin[512];
	char fout[512];
	int start, end;

	while (1) {
		memset(command, '\0', strlen(command));
		memset(reply, '\0', strlen(reply));

		if ((n = Xrecv(sock, command, 1024, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		// printf("%d\n", n);
		// TODO: receive the current CID, predict the future and send to prefetch_server 
		/*
		if (strncmp(command, "Hello from context client", 25) == 0) {
			say("Received hello from context client\n");
			char* hello = "Hello from prefetch client";
			sendCmd(sockfd_pred, hello);
		}
		*/
		if (strncmp(command, "get", 3) == 0) {
			say("Received CID request from context client\n");
			printf("%s\n", command);
			sscanf(command, "get %s %d %d", fin, &start, &end);
			//printf("get %s %d %d\n", fin, start, end);
			if (start == 0) {
				sendCmd(sockfd_pred, command);
			}
		}
	}

	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);

	// start with the current
/*
	int i, n, count = 0;
	ChunkInfo *info = NULL;
	char command[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	char *fname;
	char fin[512], fout[512];
	// char **params;
	char ad[MAX_XID_SIZE];
	char hid[MAX_XID_SIZE];

	//ChunkContext contains size, ttl, policy, and contextID which for now is PID
	ChunkContext *ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 20000000);
	if (ctx == NULL)
		die(-2, "Unable to initilize the chunking system\n");

	while (1) {
		say("waiting for command\n");
		
		memset(command, '\0', strlen(command));
		memset(reply, '\0', strlen(reply));
		
		if ((n = Xrecv(sock, command, 1024, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		// Sender does the chunking and then should start the sending commands
		if (strncmp(command, "get", 3) == 0) {
			fname = &command[4];
			say("client requested file %s\n", fname);

			if (info) {
				// clean up the existing chunks first
			}
		
			info = NULL;
			count = 0;
			
			say("chunking file %s\n", fname);
			
			// Chunking is done by the XputFile which itself uses XputChunk, and fills out the info
			if ((count = XputFile(ctx, fname, CHUNKSIZE, &info)) < 0) {
				warn("unable to serve the file: %s\n", fname);
				sprintf(reply, "FAIL: File (%s) not found", fname);
			} else {
				sprintf(reply, "OK: %d", count);
			}
			say("%s\n", reply);
			
			// Just tells the receiver how many chunks it should expect.
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		// Sending the blocks cids
		} else if (strncmp(command, "block", 5) == 0) {
			// command: "block %d:%d", offset, num
			char *start = &command[6];
			char *end = strchr(start, ':');
			if (!end) {
				// we have an invalid command, return error to client
				sprintf(reply, "FAIL: invalid block command");
			} else {
				*end = 0;
				end++;
				// FIXME: clean up naming, e is now a count
				int s = atoi(start);
				int e = s + atoi(end);
				strcpy(reply, "OK:");
				for(i = s; i < e && i < count; i++) {
					strcat(reply, " ");
					strcat(reply, info[i].cid);
				}
			}
			// print all the CIDs and send back
			printf("%s\n", reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		} else if (strncmp(command, "done", 4) == 0) {
			say("done sending file: removing the chunks from the cache\n");
			for (i = 0; i < count; i++)
				XremoveChunk(ctx, info[i].cid);
			XfreeChunkInfo(info);
			info = NULL;
			count = 0;
		}
		else if (strncmp(command, "put", 3) == 0) {
			// fname = &command[4];
			i = 0;
			say("Client wants to put a file here. cmd: %s\n" , command);
			//	put %s %s %s %s 
			i = sscanf(command, "put %s %s %s %s ", ad, hid, fin, fout);
			if (i != 4){
 				warn("Invalid put command: %s\n" , command);
			}
			getFile(sock, ad, hid, fin, fout);

		} else {
			sprintf(reply, "FAIL: invalid command (%s)\n", command);
			warn(reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		}
	}
	
	if (info)
		XfreeChunkInfo(info);
	XfreeCacheSlice(ctx);
	Xclose(sock);
	pthread_exit(NULL);
*/
}

// TBD: prefetch_client receives the xftp_client’ CID request, forward it to 
// the original service (if cache miss) and send the results back to xftp_client
void *cidRecvCmd (void *socketid) {
/*
	int i, n, count = 0;
	ChunkInfo *info = NULL;
	char command[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	char *fname;
	char fin[512], fout[512];
	// char **params;
	char ad[MAX_XID_SIZE];
	char hid[MAX_XID_SIZE];

	//ChunkContext contains size, ttl, policy, and contextID which for now is PID
	ChunkContext *ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 20000000);
	if (ctx == NULL)
		die(-2, "Unable to initilize the chunking system\n");

	while (1) {
		say("waiting for command\n");
		
		memset(command, '\0', strlen(command));
		memset(reply, '\0', strlen(reply));
		
		if ((n = Xrecv(sock, command, 1024, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		// Sender does the chunking and then should start the sending commands
		if (strncmp(command, "get", 3) == 0) {
			fname = &command[4];
			say("client requested file %s\n", fname);

			if (info) {
				// clean up the existing chunks first
			}
		
			info = NULL;
			count = 0;
			
			say("chunking file %s\n", fname);
			
			// Chunking is done by the XputFile which itself uses XputChunk, and fills out the info
			if ((count = XputFile(ctx, fname, CHUNKSIZE, &info)) < 0) {
				warn("unable to serve the file: %s\n", fname);
				sprintf(reply, "FAIL: File (%s) not found", fname);
			} else {
				sprintf(reply, "OK: %d", count);
			}
			say("%s\n", reply);
			
			// Just tells the receiver how many chunks it should expect.
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		// Sending the blocks cids
		} else if (strncmp(command, "block", 5) == 0) {
			// command: "block %d:%d", offset, num
			char *start = &command[6];
			char *end = strchr(start, ':');
			if (!end) {
				// we have an invalid command, return error to client
				sprintf(reply, "FAIL: invalid block command");
			} else {
				*end = 0;
				end++;
				// FIXME: clean up naming, e is now a count
				int s = atoi(start);
				int e = s + atoi(end);
				strcpy(reply, "OK:");
				for(i = s; i < e && i < count; i++) {
					strcat(reply, " ");
					strcat(reply, info[i].cid);
				}
			}
			// print all the CIDs and send back
			printf("%s\n", reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		} else if (strncmp(command, "done", 4) == 0) {
			say("done sending file: removing the chunks from the cache\n");
			for (i = 0; i < count; i++)
				XremoveChunk(ctx, info[i].cid);
			XfreeChunkInfo(info);
			info = NULL;
			count = 0;
		}
		else if (strncmp(command, "put", 3) == 0) {
			// fname = &command[4];
			i = 0;
			say("Client wants to put a file here. cmd: %s\n" , command);
			//	put %s %s %s %s 
			i = sscanf(command, "put %s %s %s %s ", ad, hid, fin, fout);
			if (i != 4){
 				warn("Invalid put command: %s\n" , command);
			}
			getFile(sock, ad, hid, fin, fout);

		} else {
			sprintf(reply, "FAIL: invalid command (%s)\n", command);
			warn(reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		}
	}
	
	if (info)
		XfreeChunkInfo(info);
	XfreeCacheSlice(ctx);
	Xclose(sock);
	pthread_exit(NULL);
*/
}

// Just registering the service and openning the necessary sockets
int registerStreamReceiver(char* name) {
	int sock;

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

  // read the localhost AD and HID
  if (XreadLocalHostAddr(sock, myAD, sizeof(myAD), myHID, sizeof(myHID), my4ID, sizeof(my4ID)) < 0)
		die(-1, "Reading localhost address\n");

	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	// Generate an SID to use
	if (XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}

	struct addrinfo *ai;
  // FIXED: from hardcoded SID to sid_string randomly generated
	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;
	// FIXME: NAME is hard coded
  if (XregisterName(name, dag) < 0)
		die(-1, "error registering name: %s\n", name);

	if (Xbind(sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	Graph g(dag);
	say("listening on dag: %s\n", g.dag_string().c_str());
  return sock;  
}

// FIXME: merge the two functions below
void *cidBlockingListener(void *socketid) {
  int sock = *((int*)socketid);
  int acceptSock;

  while (1) {
		say("Waiting for a client connection\n");
   		
		if ((acceptSock = Xaccept(sock, NULL, NULL)) < 0)
			die(-1, "accept failed\n");

		say("connected\n");
		
		// handle the connection in a new thread
		pthread_t client;
		pthread_create(&client, NULL, cidRecvCmd, (void *)&acceptSock);
	}
	
	Xclose(sock); // we should never reach here!
	return NULL;
}

void *ctxBlockingListener(void *socketid) {
  int sock = *((int*)socketid);
  int acceptSock;

  while (1) {
		say("Waiting for a client connection\n");
   		
		if ((acceptSock = Xaccept(sock, NULL, NULL)) < 0)
			die(-1, "accept failed\n");

		say("connected\n");
		
		// handle the connection in a new thread
		pthread_t client;
		pthread_create(&client, NULL, ctxRecvCmd, (void *)&acceptSock);
	}
	
	Xclose(sock); // we should never reach here!
	return NULL;
}

int main() {

	// TODOL change to profile
	sockfd_pred = initializeClient(prefetch_profile_name, myAD, myHID, prefetchProfileAD, prefetchProfileHID);

	sockfd_ctx = registerStreamReceiver(prefetch_ctx_name);
	ctxBlockingListener((void *)&sockfd_ctx);
	
	//sockfd_cid = registerStreamReceiver(prefetch_cid_name);
	//cidBlockingListener((void *)&sockfd_cid);	

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
}
