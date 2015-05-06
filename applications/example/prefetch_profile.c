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

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
#include "dagaddr.hpp"
#include <assert.h>

#include <sys/time.h>

#include "Xkeys.h"

#include "prefetch_utils.h"

#define MAX_XID_SIZE 100
#define VERSION "v1.0"
#define TITLE "XIA Prefetch Profiles"

#define CHUNKSIZE 1024
#define REREQUEST 3

#define NUM_CHUNKS	10
#define NUM_PROMPTS	2


char my_ad[MAX_XID_SIZE];
char my_hid[MAX_XID_SIZE];

char s_ad[MAX_XID_SIZE];
char s_hid[MAX_XID_SIZE];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char* ftp_name = "www_s.ftp.advanced.aaa.xia";
char* prefetch_client_name = "www_s.client.prefetch.aaa.xia";
char* prefetch_profile_name = "www_s.profile.prefetch..aaa.xia";
char* prefetch_pred_name = "www_s.prediction.prefetch.aaa.xia";
char* prefetch_exec_name = "www_s.executer.prefetch.aaa.xia";

int	sockfd_prefetch, sockfd_ftp;

int getSubFile(int sock, char *p_ad, char* p_hid, const char *fin, const char *fout, int start, int end);

// prefetch_client receives context updates and send predicted CID_s to prefetch_server
// handle the CID update first and location/AP later
void *RecvCmd (void *socketid) {

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
		/*
		if (strncmp(command, "Hello from prefetch client", 26) == 0)
			say("Received hello from prefetch client\n");
		*/
		printf("%s\n", command);
		// TODO: use XChunk to prefetch: establish connection and start from the next 10 chunks
		if (strncmp(command, "get", 3) == 0) {
			sscanf(command, "get %s %d %d", fin, &start, &end);
			printf("get %s %d %d\n", fin, start, end);
			getSubFile(sockfd_ftp, s_ad, s_hid, fin, fin, start, end);
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
void *BlockingListener(void *socketid) {
  int sock = *((int*)socketid);
  int acceptSock;

  while (1) {
		say("Waiting for a client connection\n");
   		
		if ((acceptSock = Xaccept(sock, NULL, NULL)) < 0)
			die(-1, "accept failed\n");

		say("connected\n");
		
		// handle the connection in a new thread
		pthread_t client;
		pthread_create(&client, NULL, RecvCmd, (void *)&acceptSock);
	}
	
	Xclose(sock); // we should never reach here!
	return NULL;
}

int getChunkCount(int sock, char *reply, int sz) {
	int n = -1;

	if ((n = Xrecv(sock, reply, sz, 0))  < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate with the server\n");
	}

	if (strncmp(reply, "OK:", 3) != 0) {
		warn( "%s\n", reply);
		return -1;
	}

	reply[n] = 0;

	return n;
}

int buildChunkDAGs(ChunkStatus cs[], char *chunks, char *p_ad, char *p_hid) {
	char *p = chunks;
	char *next;
	int n = 0;

	char *dag;
	
	// build the list of chunks to retrieve
	while ((next = strchr(p, ' '))) {
		*next = 0;
		dag = (char *)malloc(512);
		sprintf(dag, "RE ( %s %s ) CID:%s", p_ad, p_hid, p);
		// printf("built dag: %s\n", dag);
		cs[n].cidLen = strlen(dag);
		cs[n].cid = dag;
		n++;
		p = next + 1;
	}
	dag = (char *)malloc(512);
	sprintf(dag, "RE ( %s %s ) CID:%s", p_ad, p_hid, p);
	// printf("getting %s\n", p);
	cs[n].cidLen = strlen(dag);
	cs[n].cid = dag;
	n++;
	return n;
}

int getListedChunks(int csock, FILE *fd, char *chunks, char *p_ad, char *p_hid) {
	ChunkStatus cs[NUM_CHUNKS];
	char data[XIA_MAXCHUNK];
	int len;
	int status;
	int n = -1;
	
	n = buildChunkDAGs(cs, chunks, p_ad, p_hid);
	
	// NOTE: the chunk transport is not currently reliable and chunks may need to be re-requested
	// ask for the current chunk list again every REREQUEST seconds
	// chunks already in the local cache will not be refetched from the network 
	// read the the whole chunk list first before fetching
	unsigned ctr = 0;
	while (1) {
		if (ctr % REREQUEST == 0) {
			// bring the list of chunks local
			say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
			if (XrequestChunks(csock, cs, n) < 0) {
				say("unable to request chunks\n");
				return -1;
			}
			say("checking chunk status\n");
		}
		ctr++;

		status = XgetChunkStatuses(csock, cs, n);

		if (status == READY_TO_READ)
			break;

		else if (status < 0) {
			say("error getting chunk status\n");
			return -1;

		} else if (status & WAITING_FOR_CHUNK) {
			// one or more chunks aren't ready.
			say("waiting... one or more chunks aren't ready yet\n");
		
		} else if (status & INVALID_HASH) {
			die(-1, "one or more chunks has an invalid hash");
		
		} else if (status & REQUEST_FAILED) {
			die(-1, "no chunks found\n");

		} else {
			say("unexpected result\n");
		}
		sleep(1);
	}

	say("all chunks ready\n");

	for (int i = 0; i < n; i++) {
		char *cid = strrchr(cs[i].cid, ':');
		cid++;
		say("reading chunk %s\n", cid);
		if ((len = XreadChunk(csock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
			say("error getting chunk\n");
			return -1;
		}

		// write the chunk to disk
		// say("writing %d bytes of chunk %s to disk\n", len, cid);
		fwrite(data, 1, len, fd);

		free(cs[i].cid);
		cs[i].cid = NULL;
		cs[i].cidLen = 0;
	}

	return n;
}

int initializeClient(const char *name) {
	int sock, rc;
	sockaddr_x dag;
	socklen_t daglen;
	char sdag[1024];
	char IP[MAX_XID_SIZE];

	// lookup the xia service 
	daglen = sizeof(dag);
	if (XgetDAGbyName(name, &dag, &daglen) < 0)
		die(-1, "unable to locate: %s\n", name);

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");
    
	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	rc = XreadLocalHostAddr(sock, my_ad, MAX_XID_SIZE, my_hid, MAX_XID_SIZE, IP, MAX_XID_SIZE);

	if (rc < 0) {
		Xclose(sock);
		die(-1, "Unable to read local address.\n");
	} else{
		warn("My AD: %s, My HID: %s\n", my_ad, my_hid);
	}
	
	// save the AD and HID for later. This seems hacky
	// we need to find a better way to deal with this
	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
	// say("sdag = %s\n",sdag);
	char *ads = strstr(sdag,"AD:");
	char *hids = strstr(sdag,"HID:");
	// i = sscanf(ads,"%s",s_ad );
	// i = sscanf(hids,"%s", s_hid);
	
	if (sscanf(ads, "%s", s_ad ) < 1 || strncmp(s_ad, "AD:", 3) != 0) {
		die(-1, "Unable to extract AD.");
	}
		
	if (sscanf(hids,"%s", s_hid) < 1 || strncmp(s_hid, "HID:", 4) != 0) {
		die(-1, "Unable to extract AD.");
	}

	warn("Service AD: %s, Service HID: %s\n", s_ad, s_hid);
	return sock;
}

//	This is used both to put files and to get files since in case of put I still have to request the file.
//	Should be fixed with push implementation
int getSubFile(int sock, char *p_ad, char* p_hid, const char *fin, const char *fout, int start, int end) {
	int chunkSock;
	int offset;
	char cmd[512];
	char reply[512];
	int status = 0;
	
	// send the file request
	//printf("%sspace\n", fin);
	sprintf(cmd, "get %s", fin);
	sendCmd(sock, cmd);

	// get back number of chunks in the file
	if (getChunkCount(sock, reply, sizeof(reply)) < 1){
		warn("could not get chunk count. Aborting. \n");
		return -1;
	}

	// reply: OK: ***
	int count = atoi(&reply[4]);

	say("%d chunks in total\n", count);

	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");

	FILE *f = fopen(fout, "w");

	offset = start;

	struct timeval tv;
	int start_msec, temp_start_msec, temp_end_msec;
	
	if (gettimeofday(&tv, NULL) == 0)
		start_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
			
	// TODO: when to end?
	while (offset < count) {
		int num = NUM_CHUNKS;
		if (count - offset < num)
			num = count - offset;

		// tell the server we want a list of <num> cids starting at location <offset>
		printf("\nFetched chunks: %d/%d:%.1f%\n\n", offset, count, 100*(double)(offset)/count);

		sprintf(cmd, "block %d:%d", offset, num);
		
		if (gettimeofday(&tv, NULL) == 0)
			temp_start_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
					
		// send the requested CID range
		sendCmd(sock, cmd);

/*
		// say hello to the connext server
		char* hello = "Hello from context client";		
		int m = sendCmd(prefetch_ctx_sock, hello); 
		printf("%d\n");
		say("Sent hello msg\n");
*/
		if (getChunkCount(sock, reply, sizeof(reply)) < 1) {
			warn("could not get chunk count. Aborting. \n");
			return -1;
		}
		offset += NUM_CHUNKS;
		// &reply[4] are the requested CIDs  
		if (getListedChunks(chunkSock, f, &reply[4], p_ad, p_hid) < 0) {
			status= -1;
			break;
		}
		if (gettimeofday(&tv, NULL) == 0)
			temp_end_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
					
		printf("Time elapses: %.3f seconds\n", (double)(temp_end_msec - temp_start_msec)/1000);
	}
	printf("Time elapses: %.3f seconds in total.\n", (double)(temp_end_msec - start_msec)/1000);	
	fclose(f);

	if (status < 0) {
		unlink(fin);
	}

	say("Received file %s\n", fout);
	sendCmd(sock, "done");
	Xclose(chunkSock);
	return status;
}

int main() {

	sockfd_ftp = initializeClient(ftp_name); 
	sockfd_prefetch = registerStreamReceiver(prefetch_pred_name);
	BlockingListener((void *)&sockfd_prefetch);

	//getSubFile(sockfd_ftp, s_ad, s_hid, "4.png", "my4.png", 0, 1);
}
