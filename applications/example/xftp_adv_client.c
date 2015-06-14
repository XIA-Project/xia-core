#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP client"

#define POLL_USEC 1000000

using namespace std;

bool prefetch = true;

string lastSSID, currSSID;

char fin[256], fout[256];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char netHID[MAX_XID_SIZE];

char currAD[MAX_XID_SIZE];

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

char fetchAD[MAX_XID_SIZE];
char fetchHID[MAX_XID_SIZE];

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

//char prefetch_client_name[] = "www_s.client.prefetch.aaa.xia";

int ftpSock, prefetchProfileSock, netMonSock; // prefetchClientSock

bool netChange, netChangeACK;

pthread_mutex_t fileLock = PTHREAD_MUTEX_INITIALIZER;

#define BLANK 0	// initilized
#define REQ 1 	// requested
#define DONE 2	// fetched

struct chunkLog {
	int fetch; 	
	long timestamp; // when start to fetch
	string fromNet;  
};

map<string, chunkLog> chunkProfile;

vector<string> CIDs;

FILE *fd;	

vector<char *> content;
vector<int> content_len;

int thread_c = 0; // keep tracking the number of threads

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
//cerr<<"Current AD: "<<curr_ad<<endl;
/*
		if (strcmp(last_ad, curr_ad) != 0) {
			cerr<<"AD changed!\n";
			netChange = true;
			strcpy(last_ad, curr_ad);
			// wait for client to ack the network change 
			while (1) {
				if (netChangeACK == true) {
					netChange = false;
					netChangeACK = false;
				}
				usleep(100000); // every 100 ms
			}
		}
*/
		// TODO: to replace with the following:
		/*
		struct pollfd pfds[2];
		pfds[0].fd = sock;
		pfds[0].events = POLLIN;
		if ((rc = Xpoll(pfds, 1, 5000)) <= 0) {
			die(-5, "Poll returned %d\n", rc);
		}	*/			
		usleep(1000000);			
	}
	pthread_exit(NULL);
}

// 12 is the max NUM_CHUNKS to fetch at one time for 1024 K
int getFileBasic(int sock) 
{
	FILE *fd = fopen(fout, "w");

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
	vector<string> myCIDs;
	for (int i = 0; i < cid_num; i++) {
		myCIDs.push_back(string(strtok(NULL, " ")));
		//cerr<<CIDs[i]<<endl;
	}

	int chunkSock;

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

		sprintf(dag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, string2char(myCIDs[i]));
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
	Xclose(sock);	
	return status;
}

void *fetchFromAccessNet(void *) 
{
	thread_c++;
	int thread_id = thread_c;
	char *data;
	cerr<<"Thread id "<<thread_id<<": "<<"Is launched\n";
cerr<<getAD()<<endl;
	// send the registration message out
	char cmd[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];

	bool ssidChange = false;

	// open the socket 
	int prefetchProfileSock;
cerr<<"Thread id "<<thread_id<<": "<<"Looking up name of "<<getPrefetchServiceName()<<endl;
	if ((prefetchProfileSock = initStreamClient(getPrefetchServiceName(), myAD, myHID, prefetchProfileAD, prefetchProfileHID)) < 0)
		die(-1, "unable to create prefetchProfileSock\n");
cerr<<"Thread id "<<thread_id<<": "<<"prefetchProfileSock connected\n";

	memset(cmd, '\0', XIA_MAX_BUF);
	strcat(cmd, "reg");

	// find the first CID neither requested nor available as the starting point to register; TODO: assume it's linear
	unsigned int p;
	for (p = 0; p < CIDs.size(); p++) {
		if (chunkProfile[CIDs[p]].fetch == BLANK) 
			break;
	}
	cerr<<"Reg from chunk index "<<p<<endl;
	for (unsigned i = p; i < CIDs.size(); i++) {
		strcat(cmd, " ");
		strcat(cmd, string2char(CIDs[i]));
//cerr<<CIDs[i]<<endl;
	}

	// construct the reg message
	sendStreamCmd(prefetchProfileSock, cmd);
cerr<<"Thread id "<<thread_id<<": "<<"send the registration message out\n";
	// chunk fetching begins
	int chunkSock;
	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunkSock\n");

	// begin to poll the fetch chunks 
	for (unsigned int i = p; i < CIDs.size(); i++) {
		currSSID = getSSID();
		// network change before sending probe information: send registration message; TODO: make a function for that
		if (lastSSID != currSSID) {
cerr<<"Thread id "<<thread_id<<": "<<"Network changed before sending probe information, create another thread to continute\n";
			getNewAD(myAD);
			lastSSID = currSSID;
			ssidChange = true;			
			pthread_t thread_fetchFromNewAccessNet; 
			pthread_create(&thread_fetchFromNewAccessNet, NULL, fetchFromAccessNet, NULL);
			pthread_join(thread_fetchFromNewAccessNet, NULL);
			pthread_exit(NULL);
		}

		// send probe message to see what chunks are available start
		memset(cmd, '\0', XIA_MAX_BUF);
		memset(reply, '\0', XIA_MAX_BUF);

		strcat(cmd, "poll ");
		strcat(cmd, string2char(CIDs[i]));

cerr<<"Fetching chunk "<<i+1<<" / "<<CIDs.size()<<endl;
		// sending polling information
		while (1) {
			currSSID = getSSID();
			// network change before getting CIDs from polling
			if (lastSSID != currSSID) {
cerr<<"Thread id "<<thread_id<<": "<<"Network changed before sending probe information, create another thread to continute\n";
				getNewAD(currAD);
				lastSSID = currSSID;
				ssidChange = true;
				pthread_t thread_fetchFromNewAccessNet; 
				pthread_create(&thread_fetchFromNewAccessNet, NULL, fetchFromAccessNet, NULL);
				pthread_join(thread_fetchFromNewAccessNet, NULL);
				pthread_exit(NULL);
			}
			sendStreamCmd(prefetchProfileSock, cmd);
			int rcvLen = -1;
			if ((rcvLen = Xrecv(prefetchProfileSock, reply, sizeof(reply), 0))  < 0) {
				warn("socket error while waiting for data, closing connection\n");
				break;
			}
			cerr<<"Reply: "<<reply<<endl;
			// some chunks are already available to fetch
			if (strncmp(reply, "available none", 14) != 0) 
				break; 
			usleep(POLL_USEC); // probe every 100 ms
		}

		// receive and parse probe reply, set the state "REQ" chunkProfile and send available chunks request out
		strtok(reply, " "); // skip "available"
		vector<char *> CIDs_DONE;
		char* CID_DONE;
		XgetServADHID(getPrefetchServiceName(), myAD, netHID);					
		while ((CID_DONE = strtok(NULL, " ")) != NULL) {
			CIDs_DONE.push_back(CID_DONE);
			chunkProfile[string(CID_DONE)].fetch = REQ;
			chunkProfile[string(CID_DONE)].timestamp = now_msec();
			chunkProfile[string(CID_DONE)].fromNet = myAD;
		} 
		ChunkStatus cs[CIDs_DONE.size()];
		int len;
		int status = -1;
		int n = CIDs_DONE.size();

		pthread_t thread_fetchFromNewAccessNet; 

		currSSID = getSSID();

		// network change before constructing chunk request: get chunks from previous network
		if (lastSSID != currSSID) {
cerr<<"Thread id "<<thread_id<<": "<<"Network changed before constructing chunk request\n";
			getNewAD(currAD);
			lastSSID = currSSID;
			ssidChange = true;
			pthread_create(&thread_fetchFromNewAccessNet, NULL, fetchFromAccessNet, NULL);	
		}

		for (unsigned int j = 0; j < CIDs_DONE.size(); j++) {
			char *dag = (char *)malloc(512);
			sprintf(dag, "RE ( %s %s ) CID:%s", fetchAD, fetchHID, CIDs_DONE[j]);
			cs[j].cidLen = strlen(dag);
			cs[j].cid = dag; // cs[i].cid is a DAG, not just the CID
		}
		// TODO: add ftpServAD and ftpServHID as fallbacks

		unsigned ctr = 0;

		while (1) {
			currSSID = getSSID();
			// Network changed in the middle of fetching chunks
			if (lastSSID != currSSID) {
cerr<<"Thread id "<<thread_id<<": "<<"Network changed during sending chunk request\n";
				getNewAD(currAD);
				lastSSID = currSSID;
				ssidChange = true;
				pthread_create(&thread_fetchFromNewAccessNet, NULL, fetchFromAccessNet, NULL);	
			}	
			// retransmit chunks request every REREQUEST seconds if not ready
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
			data = (char*)calloc(XIA_MAXCHUNK, 1);			
			if ((len = XreadChunk(chunkSock, data, XIA_MAXCHUNK, 0, cs[j].cid, cs[j].cidLen)) < 0) {  
				say("error getting chunk\n");
				pthread_exit(NULL);
			}
			//say("writing %d bytes of chunk %s to disk\n", len, cid);
			chunkProfile[string(CIDs_DONE[j])].fetch = DONE;
			
			pthread_mutex_lock(&fileLock);
			int id = getIndex(string(CIDs_DONE[j]), CIDs);
cerr<<id<<"\t"<<CIDs_DONE[j]<<endl;
			content[id] = data;
			content_len[id] = len;
			pthread_mutex_unlock(&fileLock);
			//fwrite(data, 1, len, fd);
			free(cs[j].cid);
			cs[j].cid = NULL;
			cs[j].cidLen = 0;
		}
		if (ssidChange == true) {
cerr<<"Thread id "<<thread_id<<": "<<"Finished fetching all the chunks from old network, waiting for other threads\n";
			pthread_join(thread_fetchFromNewAccessNet, NULL);
			pthread_exit(NULL);
		}
		i = i + n - 1;
	}
	cerr<<"Finished one thread\n";
	pthread_exit(NULL);
}

int getFileAdv(int sock) 
{

	fd = fopen(fout, "w");	

	char cmd[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];

	memset(cmd, '\0', strlen(cmd));
	memset(reply, '\0', strlen(reply));
	
	int n = -1;

	strcpy(fetchAD, ftpServAD);
	strcpy(fetchHID, ftpServHID);

	// send the file request to the xftp server
	sprintf(cmd, "get %s", fin);
	sendStreamCmd(sock, cmd);

	// receive the CID list
	if ((n = Xrecv(sock, reply, sizeof(reply), 0)) < 0) {
		Xclose(sock);
		die(-1, "Unable to communicate with the server\n");
	}

	// populate the CIDs and chunkProfile
	cerr<<reply<<endl;

	char reply_arr[strlen(reply)];
	strcpy(reply_arr, reply);
cerr<<reply_arr<<endl;
	int cid_num = atoi(strtok(reply_arr, " "));
	char *tempCID;
	for (int i = 0; i < cid_num; i++) {
		tempCID = strtok(NULL, " ");
		CIDs.push_back(tempCID);
		chunkLog cl;
		cl.fetch = BLANK;
		cl.timestamp = 0;
		cl.fromNet = "";
		chunkProfile[string(tempCID)] = cl;
		content.push_back('\0');
		content_len.push_back(0);
	}
	pthread_t thread_fetchFromAccessNet; 
	pthread_create(&thread_fetchFromAccessNet, NULL, fetchFromAccessNet, NULL);
	pthread_join(thread_fetchFromAccessNet, NULL);

	for (unsigned int i = 0; i < content.size(); i++) {
		fwrite(content[i], 1, content_len[i], fd);
		free(content[i]);	
	}

	fclose(fd);
	say("Received file %s\n", fout);
	sendStreamCmd(sock, "done"); 
	Xclose(sock);

	return 0;	
}

int main(int argc, char **argv) 
{	
	while (1) {
		if (argc == 2) {
			say ("\n%s (%s): started\n", TITLE, VERSION);		
			strcpy(fin, argv[1]);
			sprintf(fout, "my%s", fin);

			// TODO: handle when SSID is null
			lastSSID = execSystem(GETSSID_CMD);
			currSSID = execSystem(GETSSID_CMD);

			ftpSock = initStreamClient(FTP_NAME, myAD, myHID, ftpServAD, ftpServHID);
			strcpy(currAD, myAD);

			//pthread_t thread_netMon;
			//pthread_create(&thread_netMon, NULL, netMon, NULL);	// TODO: improve the netMon function

			if (prefetch == false) {
				getFileBasic(ftpSock);
				return 0;
			}
			else {
				getFileAdv(ftpSock);
				return 0;
			}		
		}
		else {
			 die(-1, "Please input the filename as the second argument\n");
		}
	}
	
	return 0;
}