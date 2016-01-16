#include "stage_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP Server"

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

void *recvCmd (void *socketid) 
{
	int i, n, count = 0;
	ChunkInfo *info = NULL;
	char cmd[XIA_MAX_BUF + 4];
	char reply[XIA_MAX_BUF + 5];
	int sock = *((int*)socketid);
	char *fname;

	// ChunkContext contains size, ttl, policy, and contextID which for now is PID
	ChunkContext *ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 20000000);
	if (ctx == NULL) {
		die(-2, "Unable to initilize the chunking system\n");
	}

	while (1) {
		say("waiting for command\n");
		memset(cmd, '\0', strlen(cmd));
		memset(reply, '\0', strlen(reply));
		
		if ((n = Xrecv(sock, cmd, RECV_BUF_SIZE, 0)) < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}

		if (strncmp(cmd, "get", 3) == 0) {
			fname = &cmd[4];
			say("Client requested file %s\n", fname);

			if (info) {
				XfreeChunkInfo(info); // clean up the existing chunks first
			}
			info = NULL;
			
			say("Chunking file %s\n", fname);
			
			// Chunking is done by the XputFile which itself uses XputChunk, and fills out the info
			if ((count = XputFile(ctx, fname, CHUNKSIZE, &info)) < 0) {
				warn("unable to serve the file: %s\n", fname);
				sprintf(reply, "FAIL: File (%s) not found", fname);
			} 
			else {
				int offset = 0;
				int num;
				// Each CID's size is set to MAX_CID_NUM
				while (offset < count) {
					num = MAX_CID_NUM;
					if (count - offset < MAX_CID_NUM) {
						num = count - offset;
					}
					memset(reply, '\0', strlen(reply));
					sprintf(reply, "cont");
					for (i = offset; i < offset + num; i++) {
						strcat(reply, " ");
						strcat(reply, info[i].cid);
					}
					offset += MAX_CID_NUM;
					// Send CID list to client.
					say("Sending %d chunks of %d to %d: %s\n", num, offset, offset + num, reply);
					if (Xsend(sock, reply, strlen(reply), 0) < 0) {
						warn("unable to send reply to client\n");
						break;
					}
					hearHello(sock);
					//usleep(SCAN_DELAY_MSEC);
				}
				memset(reply, '\0', strlen(reply));
				sprintf(reply, "done");	
				if (Xsend(sock, reply, strlen(reply), 0) < 0) {
					warn("unable to send reply to client\n");
					break;
				}								
			}
		}
		// After all chunks had been sent, they can be removed from server. 	
		else if (strncmp(cmd, "done", 4) == 0) {
			say("done sending file: removing the chunks from the cache\n");
			for (i = 0; i < count; i++) {
				XremoveChunk(ctx, info[i].cid);
			}
			XfreeChunkInfo(info);
			info = NULL;
			count = 0;
		} 
		else {
			sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
			warn(reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		}
	}
	
	if (info) {
		XfreeChunkInfo(info);
	}
	XfreeCacheSlice(ctx);
	Xclose(sock);
	pthread_exit(NULL);
}

int main() 
{	
	int ftpListenSock = registerStreamReceiver(getXftpName(), myAD, myHID, my4ID);
	blockListener((void *)&ftpListenSock, recvCmd);
	return 0;
}
