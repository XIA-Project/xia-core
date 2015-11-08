#include "stage_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP client"

using namespace std;

bool stage = true;

char fin[256], fout[256];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

int ftpSock, stageManagerSock;

int getFile(int sock) 
{
	FILE *fd = fopen(fout, "w");
	int n;
	int status = -1;

	int chunkSock;
	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0) {
		die(-1, "unable to create chunk socket\n");
	}

	char cmd[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];

	vector<string> CIDs;

	// send the file request to the xftp server
	sprintf(cmd, "get %s", fin);
	sendStreamCmd(sock, cmd);

	// receive the CID list from xftp server
	while (1) {
		memset(reply, '\0', XIA_MAX_BUF);		
		if ((n = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
			Xclose(sock);
			die(-1, "Unable to communicate with the server\n");
		}
		if (strncmp(reply, "cont", 4) == 0) {
			say("Receiving partial chunk list\n");
			char reply_arr[strlen(reply+5)];
			strcpy(reply_arr, reply+5);
			char *cid;
			CIDs.push_back(strtok(reply_arr, " "));	
			while ((cid = strtok(NULL, " ")) != NULL) {
				CIDs.push_back(cid);
			}
		}

		else if (strncmp(reply, "done", 4) == 0) {
			say("Finish receiving all the CIDs\n");
			break;
		}
	}

	// update CID list to the local staging service if exists.
	if (stage) {
		if ((n = updateManifest(stageManagerSock, CIDs) < 0)) {
			Xclose(stageManagerSock);
			die(-1, "Unable to communicate with the local prefetching service\n");
		}
	}

	vector<unsigned int> chunkSize; // in bytes
	vector<long> latency;
	vector<long> chunkStartTime;
	vector<long> chunkFinishTime;

	long fetchTime = now_msec();
	long startTime = now_msec();
	unsigned int bytes = 0;

	// chunk fetching begins
	for (unsigned int i = 0; i < CIDs.size(); i++) {
		say("The number of chunks is %d.\n", CIDs.size());
		printf("Fetching chunk %u / %lu\n", i+1, CIDs.size());
		ChunkStatus cs[NUM_CHUNKS];
		char data[XIA_MAXCHUNK];
		int len;
		int status;
		int n = NUM_CHUNKS; 
		char *dag = (char *)malloc(512);

		sprintf(dag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, string2char(CIDs[i]));
		cs[0].cidLen = strlen(dag);
		cs[0].cid = dag; // cs[i].cid is a DAG, not just the CID
		unsigned ctr = 0;

		fetchTime = now_msec();
		while (1) {
			if (ctr % REREQUEST == 0) {
				// bring the list of chunks local
//say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
				if (stage) {
					if (XrequestChunkStage(stageManagerSock, cs) < 0) {
						say("unable to request chunks\n");
						return -1;
					}
					sprintf(dag, "RE ( %s %s ) CID:%s", myAD, myHID, string2char(CIDs[i]));
					cs[0].cidLen = strlen(dag);
					cs[0].cid = dag; // cs[i].cid is a DAG, not just the CID					
					if (XrequestChunks(chunkSock, cs, n) < 0) {
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
			}
			ctr++;

			status = XgetChunkStatuses(chunkSock, cs, n);

			if (status == READY_TO_READ) {
				break;
			}
			else if (status & WAITING_FOR_CHUNK) {
				say("waiting... one or more chunks aren't ready yet\n");
			}
			else if (status & INVALID_HASH) {
				die(-1, "one or more chunks has an invalid hash");
			}
			else if (status & REQUEST_FAILED) {
				die(-1, "no chunks found\n");
			}
			else if (status < 0) {
				die(-1, "error getting chunk status\n"); 
			}
			else {
				say("unexpected result\n");
			}

			usleep(CHUNK_REQUEST_DELAY_MSEC*1000);
		}

		say("Chunk is ready\n");

		if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[0].cid, cs[0].cidLen)) < 0) {
			warn("error getting chunk\n");
			return -1;
		}
		say("writing %d bytes of chunk %s to disk\n", len, string2char(CIDs[i]));
		fwrite(data, 1, len, fd);
		free(cs[0].cid);
		cs[0].cid = NULL;
		cs[0].cidLen = 0;
		bytes += len;
		chunkSize.push_back(len); 
		latency.push_back(now_msec()-fetchTime);
		chunkStartTime.push_back(fetchTime);
		chunkFinishTime.push_back(now_msec());
	}
	fclose(fd);
	long finishTime = now_msec();
	say("Received file %s at %f MB/s (Time: %lu msec, Size: %d bytes)\n", fout, (1000 * (float)bytes / 1024) / (float)(finishTime - startTime) , finishTime - startTime, bytes);
	sendStreamCmd(sock, "done"); 	// chunk fetching ends
	for (unsigned int i = 0; i < CIDs.size(); i++) {
		cout<<fout<<"\t"<<CIDs[i]<<"\t"<<chunkSize[i]<<" B\t"<<latency[i]<<"\t"<<chunkStartTime[i]<<"\t"<<chunkFinishTime[i]<<endl;
	}
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
			stageManagerSock = registerStageManager(getStageManagerName());
			if (stageManagerSock == -1) {
				say("No local staging service running\n");
				stage = false;
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
