/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP Server"

// #define DAG  "RE %s %s %s"
#define SID "SID:00000000dd41b924c1001cfa1e1117a812492435"

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char *name = "www_s.ftp.advanced.aaa.xia";

void *recvCmd (void *socketid) {
	int i, n, count = 0;
	ChunkInfo *info = NULL;
	char command[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	char *fname;

	//ChunkContext contains size, ttl, policy, and contextID which for now is PID
	ChunkContext *ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 20000000);
	if (ctx == NULL)
		die(-2, "Unable to initilize the chunking system\n");

	while (1) {
		say("waiting for command\n");
		memset(command, '\0', strlen(command));
		memset(reply, '\0', strlen(reply));
		
		if ((n = Xrecv(sock, command, 1024, 0)) < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		// Server chunk the content and then should start the sending commands
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
			} 
			else {
				sprintf(reply, "OK: %d", count);
			}
			say("%s\n", reply);
			
			// send the total number of chunks back
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		} 
		// sending the cids (info[i].cid) back
		else if (strncmp(command, "block", 5) == 0) {
			// command: "block %d:%d", offset, num
			char *start = &command[6]; // offset
			char *end = strchr(start, ':'); // get 
			if (!end) 
				sprintf(reply, "FAIL: invalid block command");			
			else {
				*end = 0;
				end++;
				// FIXME: clean up naming, e is now a count
				int s = atoi(start);
				int e = s + atoi(end);
				strcpy(reply, "OK:");
				for (i = s; i < e && i < count; i++) {
					strcat(reply, " ");
					strcat(reply, info[i].cid);
				}
			}
			printf("%s\n", reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		} 
		else if (strncmp(command, "done", 4) == 0) {
			say("done sending file: removing the chunks from the cache\n");
			for (i = 0; i < count; i++)
				XremoveChunk(ctx, info[i].cid);
			XfreeChunkInfo(info);
			info = NULL;
			count = 0;
		} 
		else {
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
}

void *blockingListener(void *socketid) {
  int sock = *((int*)socketid);
  int acceptSock;

  while (1) {
		say("Waiting for a client connection\n");
   		
		if ((acceptSock = Xaccept(sock, NULL, NULL)) < 0)
			die(-1, "accept failed\n");

		say("connected\n");
		
		// handle the connection in a new thread
		pthread_t client;
		pthread_create(&client, NULL, recvCmd, (void *)&acceptSock);
	}
	
	Xclose(sock); // we should never reach here!
	return NULL;
}

int main() {	
	int sock = registerStreamReceiver(name, myAD, myHID, my4ID);
	blockingListener((void *)&sock);
	return 0;
}
