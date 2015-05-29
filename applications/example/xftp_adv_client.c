#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP client"

using namespace std;

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];

/*
char lastAD[MAX_XID_SIZE];
char lastNetHID[MAX_XID_SIZE];

char currAD[MAX_XID_SIZE];
char currNetHID[MAX_XID_SIZE];
*/

char *lastSSID;
char *currSSID;

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

char lastFetchAD[MAX_XID_SIZE];
char lastFetchHID[MAX_XID_SIZE];

char currFetchAD[MAX_XID_SIZE];
char currFetchHID[MAX_XID_SIZE];

char prefetchClientAD[MAX_XID_SIZE];
char prefetchClientHID[MAX_XID_SIZE];

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

char ftp_name[] = "www_s.ftp.advanced.aaa.xia";
char prefetch_client_name[] = "www_s.client.prefetch.aaa.xia";
char prefetch_profile_name[] = "www_s.profile.prefetch.aaa.xia";

int ftpSock;
int prefetchClientSock;
int	prefetchProfileSock;

int getFileBasic(int sock, char *ftpServAD, char *ftpServHID, char *fin, char *fout) {
	int chunkSock;
	char cmd[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];
	int n = -1;
	int status = 0;

	// send the file request to the xftp server
	sprintf(cmd, "get %s", fin);
	sendCmd(sock, cmd);

	// receive the CID list
	if ((n = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate with the server\n");
	}

	// chunk fetching begins
	char reply_arr[strlen(reply)];
	strcpy(reply_arr, reply);
	int cid_num = atoi(strtok(reply_arr, " "));
	vector<char *> CIDs;
	for (int i = 0; i < cid_num; i++) {
		CIDs.push_back(strtok(NULL, " "));
	}
	/*
	for (int i = 0; i < cid_num; i++) {
		cout<<CIDs[i]<<endl;
	}*/
	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");
	FILE *fd = fopen(fout, "w");

	for (int i = 0; i < cid_num; i++) {
		printf("Fetching chunk %d / %d\n", i+1, cid_num);
		ChunkStatus cs[NUM_CHUNKS]; // NUM_CHUNKS == 1 for now
		char data[XIA_MAXCHUNK];
		int len;
		int status;
		int n = 1;
		char *dag = (char *)malloc(512);

		sprintf(dag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, CIDs[i]);
		cs[0].cidLen = strlen(dag);
		cs[0].cid = dag; // cs[i].cid is a DAG, not just the CID
		unsigned ctr = 0;

		while (1) {
			if (ctr % REREQUEST == 0) {
				// bring the list of chunks local
				say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
				if (XrequestChunks(chunkSock, cs, n) < 0) {
					say("unable to request chunks\n");
					return -1;
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
	sendCmd(sock, "done"); 	// chunk fetching ends
	Xclose(chunkSock);
	
	return status;
}

int getFileAdv(int sock, char *ftpServAD, char *ftpServHID, char *fin, char *fout) {
	int chunkSock;
	char cmd[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];
	int n = -1;
	int status = 0;

	strcpy(lastFetchAD, ftpServAD);
	strcpy(lastFetchHID, ftpServHID);
	strcpy(currFetchAD, ftpServAD);
	strcpy(currFetchHID, ftpServHID);

	// send the file request to the xftp server
	sprintf(cmd, "get %s", fin);
	sendCmd(sock, cmd);

	// receive the CID list
	if ((n = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate with the server\n");
	}

	// send the registration message out
	sprintf(cmd, "reg %s", reply);
	sendCmd(prefetchProfileSock, cmd);
	cerr<<"send the registration message out\n";
	// chunk fetching begins
	char reply_arr[strlen(reply)];
	strcpy(reply_arr, reply);
	int cid_num = atoi(strtok(reply_arr, " "));
	vector<char *> CIDs;
	for (int i = 0; i < cid_num; i++) {
		CIDs.push_back(strtok(NULL, " "));
	}
	/*
	for (int i = 0; i < cid_num; i++) {
		cout<<CIDs[i]<<endl;
	}*/
	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");
	FILE *fd = fopen(fout, "w");

	for (int i = 0; i < cid_num; i++) {
		// TODO: coordinate probe or not when fetching from previous network
		printf("Fetching chunk %d / %d\n", i+1, cid_num);

		currSSID = execSystem("iwgetid -r");

		// TODO: ask Dan about blocking optimization 
		if (currSSID == NULL) {
			while (1) {
				usleep(50000); // TODO: need further optimization
				currSSID = execSystem("iwgetid -r"); 
				if (currSSID != NULL) 
					break;
			}
		}
		// network change before sending probe information: send registration message; TODO: make a function for that
		if (strcmp (lastSSID, currSSID) != 0 && currSSID != NULL) {
			cerr<<"Network changed\n";
			// TODO: part of prefetch_profile_name should be SSID 
			strcpy(lastSSID, currSSID);			
			strcpy(lastFetchAD, currFetchAD);
			strcpy(lastFetchHID, currFetchHID);

			// TODO: update prefetch_profile_name
			XgetNetADHID(prefetch_profile_name, currFetchAD, currFetchHID);

			// send registration message with remaining CIDs
			memset(cmd, '\0', strlen(cmd));
			strcat(cmd, "reg ");
			int rest_cid_num = cid_num - i + 1;
			char *rest_cid_num_str;
			sprintf(rest_cid_num_str, " %d", rest_cid_num);
			strcat(cmd, rest_cid_num_str);

			for (int j = i; j < cid_num; j++) {
				strcat(cmd, " ");
				strcat(cmd, CIDs[j]);
			}
			// TODO: update prefetchProfileSock
			sendCmd(prefetchProfileSock, cmd);
			cerr<<"Send registration message with updated CID list\n";
			cerr<<cmd<<endl;
		}

		// send probe message to see what chunks are available start
		memset(cmd, '\0', strlen(cmd));
		memset(reply, '\0', strlen(reply));

		strcat(cmd, "fetch ");
		strcat(cmd, CIDs[i]);

		while (1) {
			sendCmd(prefetchProfileSock, cmd);
			int rcvLen = -1;
			if ((rcvLen = Xrecv(prefetchProfileSock, reply, sizeof(reply), 0))  < 0) {
				warn("socket error while waiting for data, closing connection\n");
				break;
			}
			cerr<<reply<<endl;
			// some chunks are already available to fetch
			if (strncmp(reply, "available none", 14) != 0) 
				break; 
			usleep(100000); // probe every 100 ms
		}
		// receive probe message to see what chunks are available ends

		strtok(reply, " "); // skip "available"
		vector<char *> CIDS_DONE;
		char* CID_DONE;
		while ((CID_DONE = strtok(NULL, " ")) != NULL) 
			CIDS_DONE.push_back(CID_DONE);

		ChunkStatus cs[CIDS_DONE.size()];
		char data[XIA_MAXCHUNK];
		int len;
		int status;
		int n = CIDS_DONE.size();

		// network change before constructing chunk request: get chunks from previous network
		if (strcmp (lastSSID, currSSID) != 0 && currSSID != NULL) {
			cerr<<"Network changed\n";
			// TODO: part of prefetch_profile_name should be SSID 
			strcpy(lastSSID, currSSID);			
			strcpy(lastFetchAD, currFetchAD);
			strcpy(lastFetchHID, currFetchHID);

			// TODO: update prefetch_profile_name
			XgetNetADHID(prefetch_profile_name, currFetchAD, currFetchHID);

			// send registration message with remaining CIDs
			memset(cmd, '\0', strlen(cmd));
			strcat(cmd, "reg ");
			// TODO: fetchAD and fetchHID should be old network for the remaining chunks
			int rest_cid_num = cid_num - i + 1;
			char *rest_cid_num_str;
			sprintf(rest_cid_num_str, " %d", rest_cid_num);
			strcat(cmd, rest_cid_num_str);

			// start from current chunk with offset n
			for (int j = i + n; j < cid_num; j++) {
				strcat(cmd, " ");
				strcat(cmd, CIDs[j]);
			}
			// TODO: update prefetchProfileSock
			sendCmd(prefetchProfileSock, cmd);
			cerr<<"Send registration message with updated CID list\n";
			cerr<<cmd<<endl;

			for (unsigned int j = 0; j < CIDS_DONE.size(); j++) {
				char *dag = (char *)malloc(512);
				sprintf(dag, "RE ( %s %s ) CID:%s", lastFetchAD, lastFetchHID, CIDS_DONE[j]);
				cs[j].cidLen = strlen(dag);
				cs[j].cid = dag; // cs[i].cid is a DAG, not just the CID
			}			
		}

		else if (strcmp (lastSSID, currSSID) != 0) {
			for (unsigned int j = 0; j < CIDS_DONE.size(); j++) {
				char *dag = (char *)malloc(512);
				sprintf(dag, "RE ( %s %s ) CID:%s", currFetchAD, currFetchHID, CIDS_DONE[j]);
				cs[j].cidLen = strlen(dag);
				cs[j].cid = dag; // cs[i].cid is a DAG, not just the CID
			}
		}
		// TODO: add ftpServAD and ftpServHID as fallbacks

		unsigned ctr = 0;

		while (1) {
			// Network changed, no need to update dest dag
			if (strcmp (lastSSID, currSSID) != 0 && currSSID != NULL) {
				cerr<<"Network changed\n";
				// TODO: part of prefetch_profile_name should be SSID 
				strcpy(lastSSID, currSSID);			
				strcpy(lastFetchAD, currFetchAD);
				strcpy(lastFetchHID, currFetchHID);

				// TODO: update prefetch_profile_name
				XgetNetADHID(prefetch_profile_name, currFetchAD, currFetchHID);

				// send registration message with remaining CIDs
				memset(cmd, '\0', strlen(cmd));
				strcat(cmd, "reg ");

				int rest_cid_num = cid_num - i + 1;
				char *rest_cid_num_str;
				sprintf(rest_cid_num_str, " %d", rest_cid_num);
				strcat(cmd, rest_cid_num_str);
				// TODO: change the starting point
				for (int j = i + n; j < cid_num; j++) {
					strcat(cmd, " ");
					strcat(cmd, CIDs[j]);
				}
				// TODO: update prefetchProfileSock
				sendCmd(prefetchProfileSock, cmd);
				cerr<<"Send registration message with updated CID list\n";
				cerr<<cmd<<endl;
			}	
			// Retransmit chunks request every REREQUEST seconds if not ready
			if (ctr % REREQUEST == 0) {
				// bring the list of chunks local
				say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
				if (XrequestChunks(chunkSock, cs, n) < 0) {
					say("unable to request chunks\n");
					return -1;
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

		for (unsigned int j = 0; j < CIDS_DONE.size(); j++) {
			if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[j].cid, cs[j].cidLen)) < 0) {
				say("error getting chunk\n");
				return -1;
			}
			//say("writing %d bytes of chunk %s to disk\n", len, cid);
			fwrite(data, 1, len, fd);		
			free(cs[j].cid);
			cs[j].cid = NULL;
			cs[j].cidLen = 0;
		}
		i = i + n;
	}
	fclose(fd);
	say("Received file %s\n", fout);
	sendCmd(sock, "done"); 
	Xclose(chunkSock);
	
	return status;
}

int main(int argc, char **argv) {	

	if (argc == 2) {
		say ("\n%s (%s): started\n", TITLE, VERSION);		
		char *fin = argv[1];
		char fout[strlen(fin)+2];
		sprintf(fout, "my%s", fin);

		ftpSock = initializeClient(ftp_name, myAD, myHID, ftpServAD, ftpServHID);

		prefetchProfileSock = initializeClient(prefetch_profile_name, myAD, myHID, prefetchProfileAD, prefetchProfileHID);		

		// TODO: handle when SSID is null
		lastSSID = execSystem("iwgetid -r");
		currSSID = execSystem("iwgetid -r");

		getFileAdv(ftpSock, ftpServAD, ftpServHID, fin, fout);
	}
	else {
		 die(-1, "Please input the filename as the second argument\n");
	}

	return 0;
}