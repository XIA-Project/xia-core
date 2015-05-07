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
#define TITLE "XIA Prefetch Executer"

char src_ad[MAX_XID_SIZE];
char src_hid[MAX_XID_SIZE];

char dst_ad[MAX_XID_SIZE];
char dst_hid[MAX_XID_SIZE];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char* ftp_name = "www_s.ftp.advanced.aaa.xia";
char* prefetch_client_name = "www_s.client.prefetch.aaa.xia";
char* prefetch_profile_name = "www_s.profile.prefetch.aaa.xia";
char* prefetch_pred_name = "www_s.prediction.prefetch.aaa.xia";
char* prefetch_exec_name = "www_s.executer.prefetch.aaa.xia";

int	sockfd_prefetch, sockfd_ftp;

int getSubFile(int sock, char *dst_ad, char* dst_hid, const char *fin, const char *fout, int start, int end);

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
			getSubFile(sockfd_ftp, dst_ad, dst_hid, fin, fin, start, end); // TODO: why fin, fin?
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

// FIXME: merge the two functions below
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
		pthread_create(&client, NULL, RecvCmd, (void *)&acceptSock);
	}
	
	Xclose(sock); // we should never reach here!
	return NULL;
}


//	This is used both to put files and to get files since in case of put I still have to request the file.
//	Should be fixed with push implementation
int getSubFile(int sock, char *dst_ad, char* dst_hid, const char *fin, const char *fout, int start, int end) {
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
		if (getListedChunks(chunkSock, f, &reply[4], dst_ad, dst_hid) < 0) {
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

	sockfd_ftp = initializeClient(ftp_name, src_ad, src_hid, dst_ad, dst_hid); 
	sockfd_prefetch = registerStreamReceiver(prefetch_pred_name, myAD, myHID, my4ID);
	blockingListener((void *)&sockfd_prefetch);

	//getSubFile(sockfd_ftp, s_ad, s_hid, "4.png", "my4.png", 0, 1);
}
