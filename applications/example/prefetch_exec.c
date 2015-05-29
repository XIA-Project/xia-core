#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Prefetch Executer"

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char ftp_name[] = "www_s.ftp.advanced.aaa.xia";
char prefetch_profile_name[] = "www_s.profile.prefetch.aaa.xia";
char prefetch_exec_name[] = "www_s.executer.prefetch.aaa.xia";

int	ftpSock, prefetchProfileSock, prefetchExecSock;

void *prefetchExecCmd (void *socketid) {

	char cmd[XIA_MAXBUF]; // receive from 
	char reply[XIA_MAXBUF]; // update the profile
	int sock = *((int*)socketid);
	int n = -1;

	while (1) {

		memset(cmd, '\0', strlen(cmd));
		memset(reply, '\0', strlen(reply));

		if ((n = Xrecv(sock, cmd, sizeof(cmd), 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}

		if (strncmp(cmd, "prefetch", 8) == 0) {
			char cmd_arr[strlen(cmd)];
			strcpy(cmd_arr, cmd);
			strtok(cmd_arr, " ");
			// vector<char *> CIDs;

			strcat(reply, "prefetched");
			// char *cid_num_str; 			
			// sprintf(cid_num_str, " %d", cid_num);
			// strcat(reply, cid_num_str);

			int chunkSock;

			if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
				die(-1, "unable to create chunk socket\n");

			char* CID;

			while ((CID = strtok(NULL, " ")) != NULL) {
				// CIDs.push_back(strtok(NULL, " "));
				// printf("Prefetching chunk %d / %d\n", i+1, cid_num);
				ChunkStatus cs[NUM_CHUNKS]; // NUM_CHUNKS == 1 for now
				char data[XIA_MAXCHUNK];
				int len;
				int status;
				int n = -1;
				char *dag = (char *)malloc(512);
				char *cid = strtok(NULL, " ");
				sprintf(dag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, cid);
				cs[0].cidLen = strlen(dag);
				cs[0].cid = dag;
				unsigned ctr = 0;

				while (1) {

					if (ctr % REREQUEST == 0) {
						say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
						if (XrequestChunks(chunkSock, cs, n) < 0) {
								die(-1, "unable to request chunks\n");
						}
						say("checking chunk status\n");
					}
					ctr++;

					status = XgetChunkStatuses(chunkSock, cs, n);
					// TODO: optimize for enlarging the window size
					if (status == READY_TO_READ) {
						strcat(reply, " ");
						strcat(reply, cid);
						break;
					}
					else if (status < 0) {
						die(-1, "error getting chunk status\n");
					} 
					else if (status & WAITING_FOR_CHUNK) 
						say("waiting... one or more chunks aren't ready yet\n");
					else if (status & INVALID_HASH) 
						die(-1, "one or more chunks has an invalid hash");
					else if (status & REQUEST_FAILED)
						die(-1, "no chunks found\n");
					else 
						say("unexpected result\n");

					sleep(1); // TODO: investigate later
				}
				say("Chunk is ready\n");

				if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[0].cid, cs[0].cidLen)) < 0) {
					die(-1, "error getting chunk\n");
				}
				// TODO: need to check the next three lines: if the chunk is removed from the cache
				free(cs[0].cid);
				cs[0].cid = NULL;
				cs[0].cidLen = 0;
			}
			sendCmd(prefetchProfileSock, reply); 	// chunk fetching ends
		}
	}

	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

int main() {

	ftpSock = initializeClient(ftp_name, myAD, myHID, ftpServAD, ftpServHID); // purpose is to get ftpServAD and ftpServHID for building chunk request
	prefetchProfileSock = initializeClient(prefetch_profile_name, myAD, myHID, prefetchProfileAD, prefetchProfileHID);	
	prefetchExecSock = registerStreamReceiver(prefetch_exec_name, myAD, myHID, my4ID);
	blockListener((void *)&prefetchExecSock, prefetchExecCmd);

	return 0;
}
