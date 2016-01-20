#include "stage_utils.h"
#include <time.h>

#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP client"

using namespace std;

bool stage = true;

char fin[256], fout[256];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

char recvBuff[MAX_XID_SIZE];
int ftpSock, stageManagerSock;

int getFile(int sock)
{
	FILE *fd = fopen(fout, "w");
	int n;
	int status = -1;
	char send_to_server_[XIA_MAX_BUF];
	memset(send_to_server_, '\0', strlen(send_to_server_));
	sprintf(send_to_server_, "finish receiveing the current CIDs.");

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
		say("In getFile, receive CID list from xftp server.\n");
		memset(reply, '\0', XIA_MAX_BUF);
		if ((n = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
			Xclose(sock);
			die(-1, "Unable to communicate with the server\n");
		}
		if (strncmp(reply, "cont", 4) == 0) {
			say("Receiving partial chunk list\n");
			char reply_arr[strlen(reply)];
			//char reply_arr[strlen(reply+5)];
			strcpy(reply_arr, reply + 5);
			char *cid;
			CIDs.push_back(strtok(reply_arr, " "));
			// register CID
			while ((cid = strtok(NULL, " ")) != NULL) {
				CIDs.push_back(cid);
			}
		}

		else if (strncmp(reply, "done", 4) == 0) {
			say("Finish receiving all the CIDs\n");
			break;
		}
		sayHello(sock, send_to_server_);
	}

	// update CID list to the local staging service if exists.
	// Send CID list to stage manager
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
		printf("Fetching chunk %u / %lu\n", i, CIDs.size() - 1);
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
		clock_t start_time = clock();
		while (1) {
			say("In while loop of getFIle.\n");
			if (ctr % REREQUEST == 0) {
				// bring the list of chunks local
//say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
				if (stage) {
					//add it to deal with the problem happened in change AP	--Lwy	1.20
					if(XreadLocalHostAddr(chunkSock,myAD,sizeof(myAD),myHID,sizeof(myHID),my4ID,sizeof(my4ID)) < 0){
            say("XreadLocalHostAddr Error\n");
	            return -1;
          }
					sprintf(dag, "RE ( %s %s ) CID:%s", myAD, myHID, string2char(CIDs[i]));
					cs[0].cidLen = strlen(dag);
					cs[0].cid = dag; // cs[i].cid is a DAG, not just the CID
					//hearHello(stageManagerSock);
					//use Unix domain socket	--Lwy	1.20
					if (XrequestChunkStage(stageManagerSock, cs) < 0) {
						say("unable to request chunks\n");
						return -1;
					}
					recv(stageManagerSock,recvBuff,MAX_XID_SIZE,0);
					//hearHello(stageManagerSock);
					//sprintf(dag, "RE ( %s %s ) CID:%s", myAD, myHID, string2char(CIDs[i]));
					//cs[0].cidLen = strlen(dag);
					//cs[0].cid = dag; // cs[i].cid is a DAG, not just the CID
					say("Requesting chunk %d/%d.\n", i, CIDs.size() - 1);
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

//cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^client: chunksock is " << chunkSock << endl;
//cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^client: cs[0].cid is " << cs[0].cid << endl;
//cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^client: cs[0].cidLen is " << cs[0].cidLen << endl;
//cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^client: n is " << n << endl;
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
		clock_t end_time = clock();
		cerr << "Running time is: "<< static_cast<double>(end_time - start_time) / CLOCKS_PER_SEC * 1000 << "ms" << endl;
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
	sendStreamCmd(sock, "done"); 	// chunk fetching ends, send command to server
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
		say("In main function of xftp_adv_client.\n");
		if (argc == 2) {
			say("\n%s (%s): started\n", TITLE, VERSION);
			strcpy(fin, argv[1]);
			sprintf(fout, "my%s", fin);
			//sprintf(fout, "/home/xia/Pictures/gugu5.jpg");
			//sprintf(fout, "/home/xia/Pictures/fb.jpg");

			ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID);
			//stageManagerSock is used to communicate with stage manager.
			//change it into Unix domain socket
			stageManagerSock = registerUnixStageManager(UNIXMANAGERSOCK);
			//stageManagerSock = registerStageManager(getStageManagerName());
			say("The current stageManagerSock is %d\n", stageManagerSock);
			if (stageManagerSock == -1) {
				say("No local staging service running\n");
				stage = false;
			}
			getFile(ftpSock);
			close(stageManagerSock);
			return 0;
		}
		else {
			 die(-1, "Please input the filename as the second argument\n");
		}
	}

	return 0;
}
