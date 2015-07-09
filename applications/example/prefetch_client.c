#include "prefetch_utils.h"

using namespace std;

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char remoteAD[MAX_XID_SIZE];
char remoteHID[MAX_XID_SIZE];
char remoteSID[MAX_XID_SIZE];

char prefetchAD[MAX_XID_SIZE];
char prefetchHID[MAX_XID_SIZE];

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

char netHID[MAX_XID_SIZE];

char currAD[MAX_XID_SIZE];

char fetchAD[MAX_XID_SIZE];
char fetchHID[MAX_XID_SIZE];

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

string lastSSID, currSSID;

// TODO: request chunk and send request, intercept request, create l lendl
// TODO: optimize sleep(1)
// TODO: trafficMon function to update prefetching window


// TODO: move retransmit request from app to xtransport?

#define BLANK 0	// initilized: by registration message
#define PENDING 1 // chunk is being fetched/prefetched 
#define READY 2	// chunk is available in the local/network cache 

struct chunkStatus {
	int fetchState;
	int prefetchState;	
	string fetchAD;
	string fetchHID;	
	long fetchStartTimestamp;
	long fetchFinishTimestamp;	
	long prefetchStartTimestamp;	
	long prefetchFinishTimestamp;	
};

map<string, vector<string> > SIDToCIDs;
map<string, map<string, chunkStatus> > SIDToProfile;
map<string, unsigned int> SIDToWindow; // prefetching window
map<string, long> SIDToTime; // store the timestamp last seen

unsigned int prefetch_chunk_num = 3; // default number of chunks to prefetch

int localPrefetchSock, ftpSock;

bool netPrefetchOn = true;

vector<char *> content;
vector<int> content_len;

int thread_c = 0; // keep tracking the number of threads

pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t windowLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t timeLock = PTHREAD_MUTEX_INITIALIZER;

// TODO: double check if all the TS be updated
// TODO: check put lock in many places
// TODO: combine different SID's chunk prefetching request; // TODO: combine different SID's chunk fetch request

//int netMonSock;
bool netChange, netChangeACK;

void regHandler(int sock, char *cmd)
{
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); 
//cerr<<"Peer SID: "<<remoteSID<<endl; 

	vector<string> CIDs = cidList(cmd);
	pthread_mutex_lock(&profileLock);	
	SIDToCIDs[remoteSID] = CIDs;
	for (unsigned int i = 0; i < CIDs.size(); i++) {
		chunkStatus cs;
		cs.fetchState = BLANK;
		cs.prefetchState = BLANK;
		cs.fetchAD = "";
		cs.fetchHID = "";
		cs.fetchStartTimestamp = 0;
		cs.fetchFinishTimestamp = 0;	
		cs.prefetchStartTimestamp = 0;	
		cs.prefetchFinishTimestamp = 0;	
		SIDToProfile[remoteSID][CIDs[i]] = cs;
	}	
	pthread_mutex_unlock(&profileLock);	

	pthread_mutex_lock(&windowLock);	
	SIDToWindow[remoteSID] = prefetch_chunk_num; // TODO: to optimize
	pthread_mutex_unlock(&windowLock);

	pthread_mutex_lock(&timeLock);	
	SIDToTime[remoteSID] = now_msec();
	pthread_mutex_unlock(&timeLock);
/*
cerr<<"\nPrint profile table in reg_handler\n";
	for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
		for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
		}
	}	
*/
cerr<<"Finish reg"<<endl;

	return;
}

// TODO: change from 0 to 1 or intercept if it has been requested, 
void prefetchHandler(int sock, char *cmd) {
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); 
	size_t cidLen;
	char *cidDag = (char *)malloc(512);
	char *CID = (char *)malloc(512); // TODO: optimize the size?
	sscanf(cmd, "fetch %ld %s", &cidLen, cidDag);
	sscanf(cidDag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, CID);
	// TODO: if existed, do we need to update timestamp?
	SIDToProfile[remoteSID][CID].fetchState = PENDING;
	SIDToProfile[remoteSID][CID].fetchStartTimestamp = now_msec();
	return;
}

// TODO: mobility: when net changed, setup proper stop condition and create a new thread; update time
// TODO: scheduling 
// TODO: read the list from data structure shared with handlers 
void *prefetchData(void *) 
{
	// communite with in-net prefetching service; TODO: handle no in-net prefetching service
	int netPrefetchSock = registerPrefetchService(getPrefetchServiceName(), myAD, myHID, prefetchAD, prefetchHID); 
	if (netPrefetchSock == -1) {
		netPrefetchOn = false;
		// TODO:??
	}

	while (1) {
		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {

			if ((*I).second.size() > 0) {

				char cmd[XIA_MAX_BUF];
				char reply[XIA_MAX_BUF];
				int n = -1;
				memset(cmd, '\0', strlen(cmd));
				memset(reply, '\0', strlen(reply));
				
				string SID = (*I).first;
//cerr<<"SID: "<<SID<<"chunks to be pulled from the queue\n";				
				vector<string> CIDs;
				// find the right list to fetch
				unsigned int s = -1;
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
					if (SIDToProfile[SID][(*I).second[i]].fetchState != BLANK) {
						s = i;
						break;
					}
				}
				unsigned e = s + SIDToWindow[SID];
				if (e > (*I).second.size()) {
					e = (*I).second.size();
				}
				pthread_mutex_lock(&profileLock);	
				// TODO: what if all chunks are fetched;
				for (unsigned int i = s; i < e; i++) {
					if (SIDToProfile[SID][(*I).second[i]].prefetchState == BLANK) {
						CIDs.push_back((*I).second[i]);
						SIDToProfile[SID][(*I).second[i]].prefetchState = PENDING;					
						SIDToProfile[SID][(*I).second[i]].fetchAD = prefetchAD;
						SIDToProfile[SID][(*I).second[i]].fetchAD = prefetchHID;
					}
				}
				sprintf(cmd, "prefetch");
				for (unsigned i = 0; i < CIDs.size(); i++) {
					strcat(cmd, " ");
					strcat(cmd, string2char(CIDs[i]));
				}				
				sendStreamCmd(netPrefetchSock, cmd);

				// receive the CID list
				if ((n = Xrecv(netPrefetchSock, reply, sizeof(reply), 0)) < 0) {
					Xclose(netPrefetchSock);
					die(-1, "Unable to communicate with the server\n");
				}
cerr<<reply<<endl;
				if (strncmp(reply, "ready", 5) == 0) {
					vector<string> CIDs_prefetched = cidList(reply+6);
					for (unsigned i = 0; i < CIDs_prefetched.size(); i++) {
						SIDToProfile[SID][CIDs_prefetched[i]].prefetchState = READY;
					}
				}	
				pthread_mutex_unlock(&profileLock);					
			}
		}
	}
	pthread_exit(NULL);
}

// data plane of prefetching: find the pending chunk, construct the dag and send the pending chunk request, 
void *fetchData(void *) 
{
	while (1) {
		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {
			if ((*I).second.size() > 0) {
				string SID = (*I).first;
//cerr<<"SID: "<<SID<<"chunks to be pulled from the queue\n";				
				vector<string> CIDs;
				// find the CID to fetch behalf of the xftp client
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
					if (SIDToProfile[SID][(*I).second[i]].fetchState == PENDING && SIDToProfile[SID][(*I).second[i]].prefetchState == READY) {
						CIDs.push_back((*I).second[i]);
					}
				}				
				int chunkSock;

				if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
					die(-1, "unable to create chunk socket\n");

				// TODO: check the size is smaller than the max chunk to fetch at one time
				ChunkStatus cs[CIDs.size()];
				int status;
				int n = CIDs.size();

				for (unsigned int i = 0; i < CIDs.size(); i++) {
					char *dag = (char *)malloc(512);
					sprintf(dag, "RE ( %s %s ) CID:%s", string2char(SIDToProfile[SID][CIDs[i]].fetchAD), string2char(SIDToProfile[SID][CIDs[i]].fetchHID), string2char(CIDs[i]));
					cs[i].cidLen = strlen(dag);
					cs[i].cid = dag; // cs[i].cid is a DAG, not just the CID
				}

				unsigned ctr = 0;

				while (1) {
					// Retransmit chunks request every REREQUEST seconds if not ready
					if (ctr % REREQUEST == 0) {
						// bring the list of chunks local
						say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
						if (XrequestChunks(chunkSock, cs, n) < 0) {
							say("unable to request chunks\n");
							pthread_exit(NULL); // TODO: check again
						}
						say("checking chunk status\n");
					}
					ctr++;

					status = XgetChunkStatuses(chunkSock, cs, n);

					if (status == READY_TO_READ) {
						pthread_mutex_lock(&profileLock);	
						for (unsigned int i = 0; i < CIDs.size(); i++) {
							SIDToProfile[SID][CIDs[i]].fetchState = READY;
						}
						pthread_mutex_unlock(&profileLock);		
						break;
					}
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
/* don't need to read data
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
					if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
						say("error getting chunk\n");
						pthread_exit(NULL); // TODO: check again
					}
					else {
						SIDToProfile[SID][(*I).second[i]] = true; 
*/						
//cerr<<"update chunks DONE information...\n";						
						/*
						for (unsigned int j = 0; j < SIDToProfile[SID].size(); j++) {
							if (SIDToProfile[SID][j].CID == (*I).second[i]) 
								SIDToProfile[SID][j].prefetch = true;
						}	
						*/						
						/*
					}
				}
				(*I).second.clear();*/
				// TODO: timeout the chunks by free(cs[i].cid); cs[j].cid = NULL; cs[j].cidLen = 0;			
			}
		} 
	}
	// 	big loop, SIDToProfile[remoteSID][CID].fetchState = PENDING and prefetch state == done, construct dag, fetch data, update
	pthread_exit(NULL);
}

void *clientCmd(void *socketid) 
{
	char cmd[XIA_MAXBUF];
	int sock = *((int*)socketid);
	int n = -1;

	while (1) {
		memset(cmd, '\0', XIA_MAXBUF);
		if ((n = Xrecv(sock, cmd, sizeof(cmd), 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		} 
		// reg msg from xftp client: reg CID1, CID2, ... CIDn
		if (strncmp(cmd, "reg", 3) == 0) {
			say("Receive a chunk list registration message\n");
			regHandler(sock, cmd+4);
		}
		// chunk request from xftp client: fetch CID
		else if (strncmp(cmd, "fetch", 5) == 0) {
			say("Receive a chunk request\n");
			prefetchHandler(sock, cmd+6);
		}	
	}

	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

// TODO: make it as a library
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

		// TODO: to replace with the following:
		//struct pollfd pfds[2];
		//pfds[0].fd = sock;
		//pfds[0].events = POLLIN;
		//if ((rc = Xpoll(pfds, 1, 5000)) <= 0) {
		//	die(-5, "Poll returned %d\n", rc);
		//}				
		usleep(1000000);			
	}
	pthread_exit(NULL);
}

// TODO: pktMon
void *windowPred(void *) 
{
	// go through all the SID
	while (1) {
		//prefetch_chunk_num = 3;		
	}
	pthread_exit(NULL);
}

void *profileMgt(void *) 
{
	while (1) {
		for (map<string, long>::iterator I = SIDToTime.begin(); I != SIDToTime.end(); ++I) {
			if (now_msec() - (*I).second >= PURGE_SEC * 1000) {
				string SID = (*I).first;
				cerr<<"Deleting "<<SID<<endl;
				pthread_mutex_lock(&timeLock);
				SIDToTime.erase(SID);
				pthread_mutex_unlock(&timeLock);
				pthread_mutex_lock(&profileLock);
				SIDToProfile.erase(SID);
				SIDToCIDs.erase(SID);
				pthread_mutex_unlock(&profileLock);
				pthread_mutex_lock(&windowLock);
				SIDToWindow.erase(SID);
				pthread_mutex_unlock(&windowLock);
			}
		}	
		sleep(10);
	}
	pthread_exit(NULL);	
}

int main() 
{
	pthread_t thread_netMon, thread_windowPred, thread_mgt, thread_fetchData, thread_prefetchData;
	pthread_create(&thread_netMon, NULL, netMon, NULL);	// TODO: improve the netMon function
	pthread_create(&thread_windowPred, NULL, windowPred, NULL);	// TODO: improve the netMon function
	pthread_create(&thread_mgt, NULL, profileMgt, NULL); // dequeue, prefetch and update profile	
	pthread_create(&thread_fetchData, NULL, fetchData, NULL);		
	pthread_create(&thread_prefetchData, NULL, prefetchData, NULL);	

	ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID); // get ftpServAD and ftpServHID for building chunk request
	// TODO: handle when SSID is null
	lastSSID = execSystem(GETSSID_CMD);
	currSSID = execSystem(GETSSID_CMD);
	strcpy(currAD, myAD);

	localPrefetchSock = registerStreamReceiver(getPrefetchClientName(), myAD, myHID, my4ID); // communicate with client app
	blockListener((void *)&localPrefetchSock, clientCmd);

	return 0;	
}

/*
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
*/