#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP client"

using namespace std;

bool prefetch = true;

char fin[256], fout[256];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

int ftpSock, prefetchClientSock;

// TODO: move retransmit request from app to xtransport?
// TODO: optimize sleep(1)

// 12 is the max NUM_CHUNKS to fetch at one time for 1024 K
int getFile(int sock) 
{
	FILE *fd = fopen(fout, "w");

	char cmd[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];
	int n = -1;
	int status = 0;
	vector<string> CIDs;

	// send the file request to the xftp server
	sprintf(cmd, "get %s", fin);
	sendStreamCmd(sock, cmd);

	// receive the CID list from xftp server
	if ((n = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
		Xclose(sock);
		die(-1, "Unable to communicate with the server\n");
	}

	char reply_arr[strlen(reply)];
	strcpy(reply_arr, reply);
	int cid_num = atoi(strtok(reply_arr, " "));
	for (int i = 0; i < cid_num; i++) {
		CIDs.push_back(string(strtok(NULL, " ")));
	}

	// update CID list to the locali prefetching service.
	if (prefetch) {
		if ((n = updateManifest(prefetchClientSock, CIDs) < 0)) {
			Xclose(prefetchClientSock);
			die(-1, "Unable to communicate with the local prefetching service\n");
		}
	}
	int chunkSock;

	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0) {
		die(-1, "unable to create chunk socket\n");
	}

	// chunk fetching begins
	for (int i = 0; i < cid_num; i++) {
		printf("Fetching chunk %d / %d\n", i+1, cid_num);
		ChunkStatus cs[NUM_CHUNKS]; // NUM_CHUNKS == 1 for now
		char data[XIA_MAXCHUNK];
		int len;
		int status;
		int n = 1;
		char *dag = (char *)malloc(512);

		sprintf(dag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, string2char(CIDs[i]));
		cs[0].cidLen = strlen(dag);
		cs[0].cid = dag; // cs[i].cid is a DAG, not just the CID
		unsigned ctr = 0;

		while (1) {
			if (ctr % REREQUEST == 0) {
				// bring the list of chunks local
				say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);

				if (prefetch) {
					if (XrequestChunkPrefetch(prefetchClientSock, cs) < 0) {
						say("unable to request chunks\n");
						return -1;
					}
				}
				else {
					if (XrequestChunks(chunkSock, cs, n) < 0) {
						say("unable to request chunks\n");
						return -1;
					}
				}
				say("checking chunk status\n");
			}
			ctr++;

			status = XgetChunkStatuses(chunkSock, cs, n);

			if (status == READY_TO_READ)
				break;
			else if (status < 0) 
				die(-1, "error getting chunk status\n"); 
			else if (status & WAITING_FOR_CHUNK) 
				say("waiting... one or more chunks aren't ready yet\n");
			else if (status & INVALID_HASH) 
				die(-1, "one or more chunks has an invalid hash");
			else if (status & REQUEST_FAILED)
				die(-1, "no chunks found\n");
			else 
				say("unexpected result\n");

			sleep(1);
		}

		say("Chunk is ready\n");

		if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[0].cid, cs[0].cidLen)) < 0) {
			say("error getting chunk\n");
			return -1;
		}
		//say("writing %d bytes of chunk %s to disk\n", len, cid);
		fwrite(data, 1, len, fd);		
		free(cs[0].cid);
		cs[0].cid = NULL;
		cs[0].cidLen = 0;
	}
	fclose(fd);
	say("Received file %s\n", fout);
	sendStreamCmd(sock, "done"); 	// chunk fetching ends
	Xclose(chunkSock);
	Xclose(sock);	
	return status;
}

int main(int argc, char **argv) 
{	
	while (1) {
		if (argc == 2) {
			say("\n%s (%s): started\n", TITLE, VERSION);		
			strcpy(fin, argv[1]);
			sprintf(fout, "my%s", fin);

			ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID);

			prefetchClientSock = registerPrefetchClient(getPrefetchClientName());

			if (prefetchClientSock == -1) {
				say("No local prefetching service running\n");
				prefetch = false;
			}

			getFile(ftpSock);

			return 0;
		}
		else {
			 die(-1, "Please input the filename as the second argument\n");
		}
	}
	
	return 0;
}
