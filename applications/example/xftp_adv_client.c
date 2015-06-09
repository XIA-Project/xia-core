#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP client"

#define POLL_USEC 1000000

using namespace std;

bool prefetch = true;
bool wireless = true;

string lastSSID, currSSID;

FILE *fd;

char fin[256], fout[256];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char netHID[MAX_XID_SIZE];

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

int ftpSock, prefetchProfileSock, netMonSock; // prefetchClientSock

bool netChange, netChangeACK;

#define BLANK 0	// initilized
#define REQ 1 	// requested
#define DONE 2	// fetched

struct chunkLog {
	int fetch; 	
	long timestamp; // when start to fetch
	string fromNet;  
};

map<string, chunkLog > chunkProfile;

/*
struct addrinfo *ai;
sockaddr_x *sa;
*/

// TODO: be careful to update prefetchProfileSock for sendCmd(prefetchProfileSock, cmd);
// format: rootname.SSID
char *getPrefetchProfileName() 
{
	if (wireless == false)
		return prefetch_profile_name;
	else {
		string prefetch_profile_name_local = string(prefetch_profile_name) + "." + execSystem(GETSSID);
		return string2char(prefetch_profile_name_local);
	}
} 

void *netMon(void *) 
{
	int sock; 

	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	netChange = false;
	netChangeACK = false;
	char last_ad[MAX_XID_SIZE], curr_ad[MAX_XID_SIZE], hid[MAX_XID_SIZE], ip[MAX_XID_SIZE];

	if (XreadLocalHostAddr(sock, curr_ad, sizeof(curr_ad), hid, sizeof(hid), ip, sizeof(ip)) < 0)
		die(-1, "Reading localhost address\n");

	strcpy(last_ad, curr_ad);

	while (1) 
	{
		if (XreadLocalHostAddr(sock, curr_ad, sizeof(curr_ad), hid, sizeof(hid), ip, sizeof(ip)) < 0)
			die(-1, "Reading localhost address\n");
		cerr<<"Current AD: "<<curr_ad<<endl;
		if (strcmp(last_ad, curr_ad) != 0) {
			cerr<<"AD changed!\n";
			netChange = true;
			strcpy(last_ad, curr_ad);
			while (1) {
				if (netChangeACK == true) {
					netChange = false;
					netChangeACK = false;
				}
				usleep(1000000);
			}
		}

		// TODO: to replace with the following:
		/*
		struct pollfd pfds[2];
		pfds[0].fd = sock;
		pfds[0].events = POLLIN;
		if ((rc = Xpoll(pfds, 1, 5000)) <= 0) {
			die(-5, "Poll returned %d\n", rc);
		}	*/			
	}
	pthread_exit(NULL);
}

// 12 is the max NUM_CHUNKS to fetch at one time for 1024 K
int getFileBasic(int sock) 
{
	int chunkSock;
	char cmd[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];
	int n = -1;
	int status = 0;

	// send the file request to the xftp server
	sprintf(cmd, "get %s", fin);
	sendStreamCmd(sock, cmd);

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
	sendStreamCmd(sock, "done"); 	// chunk fetching ends
	Xclose(chunkSock);
	
	return status;
}

void * getFileAdv(void *socketid) 
{
	int sock = *((int*)socketid);
	int chunkSock;
	char cmd[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];
	int n = -1;

	strcpy(lastFetchAD, ftpServAD);
	strcpy(lastFetchHID, ftpServHID);
	strcpy(currFetchAD, ftpServAD);
	strcpy(currFetchHID, ftpServHID);

	// send the file request to the xftp server
	sprintf(cmd, "get %s", fin);
	sendStreamCmd(sock, cmd);

	// receive the CID list
	if ((n = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate with the server\n");
	}

	// send the registration message out
	sprintf(cmd, "reg %s", reply);
	sendStreamCmd(prefetchProfileSock, cmd);
	cerr<<"send the registration message out\n";
	// chunk fetching begins
	char reply_arr[strlen(reply)];
	strcpy(reply_arr, reply);
	int cid_num = atoi(strtok(reply_arr, " "));
	vector<char *> CIDs;
	char *tempCID;
	for (int i = 0; i < cid_num; i++) {
		tempCID = strtok(NULL, " ");
		CIDs.push_back(tempCID);
		chunkLog cl;
		cl.fetch = 0;
		cl.timestamp = 0;
		cl.fromNet = "";
		chunkProfile[string(tempCID)] = cl; 
	}
	/*
	for (int i = 0; i < cid_num; i++) {
		cout<<CIDs[i]<<endl;
	}*/
	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");

	for (int i = 0; i < cid_num; i++) {
		// TODO: coordinate probe or not when fetching from previous network
		printf("Fetching chunk %d / %d\n", i+1, cid_num);

		currSSID = execSystem(GETSSID);

		// TODO: ask Dan about blocking optimization 
		if (currSSID.empty()) {
			cerr<<"No network\n";
			while (1) {
				usleep(50000); // TODO: need further optimization
				currSSID = execSystem(GETSSID); 
				if (!currSSID.empty()) {
					cerr<<"Network back\n";
					break;
				}
			}
		}
		// network change before sending probe information: send registration message; TODO: make a function for that
		if (lastSSID != currSSID && !currSSID.empty()) {
			cerr<<"Network changed before sending probe information\n";
			// TODO: part of prefetch_profile_name should be SSID 
			lastSSID = currSSID;			
			strcpy(lastFetchAD, currFetchAD);
			strcpy(lastFetchHID, currFetchHID);

			cerr<<"New prefetch profile name: "<<getPrefetchProfileName()<<endl;

			XgetNetADHID(getPrefetchProfileName(), currFetchAD, currFetchHID);

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
			prefetchProfileSock = initStreamClient(getPrefetchProfileName(), myAD, myHID, prefetchProfileAD, prefetchProfileHID);		
			cerr<<"Ready to send registration message with updated CID list\n"<<cmd<<endl;
			sendStreamCmd(prefetchProfileSock, cmd);
			cerr<<"Sent registration message with updated CID list\n";
			cerr<<cmd<<endl;
		}

		// send probe message to see what chunks are available start
		memset(cmd, '\0', strlen(cmd));
		memset(reply, '\0', strlen(reply));

		strcat(cmd, "poll ");
		strcat(cmd, CIDs[i]);

		while (1) {
			// TODO: check network changed? if so break and send reg message
			sendStreamCmd(prefetchProfileSock, cmd);
			int rcvLen = -1;
			if ((rcvLen = Xrecv(prefetchProfileSock, reply, sizeof(reply), 0))  < 0) {
				warn("socket error while waiting for data, closing connection\n");
				break;
			}
			cerr<<reply<<endl;
			// some chunks are already available to fetch
			if (strncmp(reply, "available none", 14) != 0) 
				break; 
			usleep(POLL_USEC); // probe every 100 ms
		}

		// receive and parse probe reply, update chunkProfile and send available chunks request out
		strtok(reply, " "); // skip "available"
		vector<char *> CIDs_DONE;
		char* CID_DONE;
		while ((CID_DONE = strtok(NULL, " ")) != NULL) {
			CIDs_DONE.push_back(CID_DONE);
			chunkProfile[string(CID_DONE)].fetch = REQ;
			chunkProfile[string(CID_DONE)].timestamp = now_msec();
			XgetNetADHID(getPrefetchProfileName(), myAD, netHID);			
			chunkProfile[string(CID_DONE)].fromNet = myAD;
		} 

		ChunkStatus cs[CIDs_DONE.size()];
		char data[XIA_MAXCHUNK];
		int len;
		int status = -1;
		int n = CIDs_DONE.size();

		currSSID = execSystem(GETSSID);

		// TODO: ask Dan about blocking optimization 
		if (currSSID.empty()) {
			cerr<<"No network\n";
			while (1) {
				usleep(50000); // TODO: need further optimization
				currSSID = execSystem(GETSSID); 
				if (!currSSID.empty()) {
					cerr<<"Network back\n";
					break;
				}
			}
		}

		// network change before constructing chunk request: get chunks from previous network
		if (lastSSID != currSSID && !currSSID.empty()) {
			cerr<<"Network changed before constructing chunk request\n";
			lastSSID = currSSID;			
			strcpy(lastFetchAD, currFetchAD);
			strcpy(lastFetchHID, currFetchHID);

			XgetNetADHID(getPrefetchProfileName(), currFetchAD, currFetchHID);

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

			// update prefetchProfileSock
			cerr<<"Updating prefetchProfileSock...\n";			
			prefetchProfileSock = initStreamClient(getPrefetchProfileName(), myAD, myHID, prefetchProfileAD, prefetchProfileHID);		
			cerr<<"Updated prefetchProfileSock\n";			
			sendStreamCmd(prefetchProfileSock, cmd);
			cerr<<"Send registration message with updated CID list\n";
			cerr<<cmd<<endl;

			for (unsigned int j = 0; j < CIDs_DONE.size(); j++) {
				char *dag = (char *)malloc(512);
				sprintf(dag, "RE ( %s %s ) CID:%s", lastFetchAD, lastFetchHID, CIDs_DONE[j]);
				cs[j].cidLen = strlen(dag);
				cs[j].cid = dag; // cs[i].cid is a DAG, not just the CID
			}			
		}

		else if (lastSSID == currSSID) {
			for (unsigned int j = 0; j < CIDs_DONE.size(); j++) {
				char *dag = (char *)malloc(512);
				sprintf(dag, "RE ( %s %s ) CID:%s", currFetchAD, currFetchHID, CIDs_DONE[j]);
				cs[j].cidLen = strlen(dag);
				cs[j].cid = dag; // cs[i].cid is a DAG, not just the CID
			}
		}
		// TODO: add ftpServAD and ftpServHID as fallbacks

		unsigned ctr = 0;

		while (1) {
			currSSID = execSystem(GETSSID);
			if (currSSID.empty()) {
				cerr<<"No network\n";
				while (1) {
					usleep(50000); // TODO: need further optimization
					currSSID = execSystem(GETSSID); 
					if (!currSSID.empty()) {
						cerr<<"Network back\n";
						break;
					}
				}
			}
			// Network changed, no need to update dest dag
			if (lastSSID != currSSID && !currSSID.empty()) {
				cerr<<"Network changed during sending chunk request\n";
				lastSSID = currSSID;
				strcpy(lastFetchAD, currFetchAD);
				strcpy(lastFetchHID, currFetchHID);

				XgetNetADHID(getPrefetchProfileName(), currFetchAD, currFetchHID);

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
				// update prefetchProfileSock
				prefetchProfileSock = initStreamClient(getPrefetchProfileName(), myAD, myHID, prefetchProfileAD, prefetchProfileHID);						
				sendStreamCmd(prefetchProfileSock, cmd);
				cerr<<"Send registration message with updated CID list\n";
				cerr<<cmd<<endl;
			}	
			// Retransmit chunks request every REREQUEST seconds if not ready
			if (ctr % REREQUEST == 0) {
				// bring the list of chunks local
				say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
				if (XrequestChunks(chunkSock, cs, n) < 0) {
					say("unable to request chunks\n");
					pthread_exit(NULL);
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

			usleep(1000000); 
		}

		say("Chunk is ready\n");

		for (unsigned int j = 0; j < CIDs_DONE.size(); j++) {
			if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[j].cid, cs[j].cidLen)) < 0) {
				say("error getting chunk\n");
				pthread_exit(NULL);
			}
			//say("writing %d bytes of chunk %s to disk\n", len, cid);
			chunkProfile[string(CIDs_DONE[j])].fetch = DONE;			
			fwrite(data, 1, len, fd);	
			free(cs[j].cid);
			cs[j].cid = NULL;
			cs[j].cidLen = 0;
		}
		i = i + n - 1;
	}
	fclose(fd);
	say("Received file %s\n", fout);
	sendStreamCmd(sock, "done"); 
	Xclose(chunkSock);
	
	pthread_exit(NULL);
}

int main(int argc, char **argv) 
{	
	while (1) {

		if (argc == 2) {
			say ("\n%s (%s): started\n", TITLE, VERSION);		
			//char *fin = argv[1];
			//char fout[strlen(fin)+2];
			strcpy(fin, argv[1]);
			sprintf(fout, "my%s", fin);

			fd = fopen(fout, "w");

			// TODO: handle when SSID is null
			lastSSID = execSystem(GETSSID);
			currSSID = execSystem(GETSSID);

			ftpSock = initStreamClient(ftp_name, myAD, myHID, ftpServAD, ftpServHID);

			pthread_t thread_netMon, thread_getFile;
			pthread_create(&thread_netMon, NULL, netMon, NULL);	// TODO: improve the netMon function

			if (prefetch == true) {
				prefetchProfileSock = initStreamClient(getPrefetchProfileName(), myAD, myHID, prefetchProfileAD, prefetchProfileHID);
				//prefetchProfileSock = initDatagramClient(getPrefetchProfileName(), ai, sa);			
				pthread_create(&thread_getFile, NULL, getFileAdv, (void *)&ftpSock);
				//getFileAdv(ftpSock);
			}
			else {
				getFileBasic(ftpSock);
			}
		}
		else {
			 die(-1, "Please input the filename as the second argument\n");
		}
	}

	return 0;
}

		///////////////////////////////
		/*
		struct addrinfo *ai;
		sockaddr_x *sa;

		int pktSize = 512;

		if (Xgetaddrinfo(TEST_NAME, NULL, NULL, &ai) != 0)
			die(-1, "unable to lookup name %s\n", TEST_NAME);
		sa = (sockaddr_x*)ai->ai_addr;

		Graph g(sa);
		printf("\n%s\n", g.dag_string().c_str());

		int ssock;
		if ((ssock = Xsocket(AF_XIA, SOCK_DGRAM, 0)) < 0) {
			die(-2, "unable to create the socket\n");
		}
		say("Xsock %4d created\n", ssock);

		int size;
		int sent, received, rc;
		char buf1[XIA_MAXBUF], buf2[XIA_MAXBUF];

		if (pktSize == 0)
			size = (rand() % XIA_MAXBUF) + 1;
		else
			size = pktSize;
		randomString(buf1, size);

		if ((sent = Xsendto(ssock, buf1, size, 0, (sockaddr*)sa, sizeof(sockaddr_x))) < 0) {
			die(-4, "Send error %d on socket %d\n", errno, ssock);
		}

		say("Xsock %4d sent %d bytes\n", ssock, sent);

		struct pollfd pfds[2];
		pfds[0].fd = ssock;
		pfds[0].events = POLLIN;
		if ((rc = Xpoll(pfds, 1, 5000)) <= 0) {
			die(-5, "Poll returned %d\n", rc);
		}

		memset(buf2, 0, sizeof(buf2));
		if ((received = Xrecvfrom(ssock, buf2, sizeof(buf2), 0, NULL, NULL)) < 0)
			die(-5, "Receive error %d on socket %d\n", errno, ssock);

		say("Xsock %4d received %d bytes\n", ssock, received);

		if (sent != received || strcmp(buf1, buf2) != 0) {
			warn("Xsock %4d received data different from sent data! (bytes sent/recv'd: %d/%d)\n",
					ssock, sent, received);
		}
		///////////////////////////////
		*/