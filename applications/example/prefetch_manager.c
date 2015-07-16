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

struct chunkProfile {
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
map<string, map<string, chunkProfile> > SIDToProfile;
map<string, unsigned int> SIDToWindow; // prefetching window
map<string, long> SIDToTime; // store the timestamp last seen

int localPrefetchSock, ftpSock;

bool netPrefetchOn = true;

vector<char *> content;
vector<int> content_len;

int thread_c = 0; // keep tracking the number of connections with 

pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t windowLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t timeLock = PTHREAD_MUTEX_INITIALIZER;

//int netMonSock;
bool netChange, netChangeACK;

// TODO: mem leak
void regHandler(int sock, char *cmd)
{
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); 
//cerr<<"Peer SID: "<<remoteSID<<endl; 
//cerr<<cmd<<endl;

	pthread_mutex_lock(&windowLock);	
	SIDToWindow[remoteSID] = PREFETCH_WIN_INIT;
	pthread_mutex_unlock(&windowLock);

	pthread_mutex_lock(&timeLock);	
	SIDToTime[remoteSID] = now_msec();
	pthread_mutex_unlock(&timeLock);

	vector<string> CIDs = strVector(cmd);
cerr<<"Ready to enter profileLock"<<endl;		

	SIDToCIDs[remoteSID] = CIDs;

	for (unsigned int i = 0; i < CIDs.size(); i++) {
		chunkProfile cp;
		cp.fetchState = BLANK;
		cp.prefetchState = BLANK;
		cp.fetchAD = "";
		cp.fetchHID = "";
		cp.fetchStartTimestamp = 0;
		cp.fetchFinishTimestamp = 0;	
		cp.prefetchStartTimestamp = 0;	
		cp.prefetchFinishTimestamp = 0;	
		pthread_mutex_lock(&profileLock);
		SIDToProfile[remoteSID][CIDs[i]] = cp;
		pthread_mutex_unlock(&profileLock);
	}	
cerr<<"Quit the profileLock and finish reg"<<endl;

	return;
}

void prefetchHandler(int sock, char *cmd) {

cerr<<"Receving Command: "<<cmd<<endl; 
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); 
	size_t cidLen;
	//char *cidDag = (char *)malloc(512);
	char *CID = (char *)malloc(512); 
	// TODO: right now it's hacky, need to fix the way reading XIDs when including fallback DAG
	sscanf(cmd, "%ld RE ( %s %s ) CID:%s", &cidLen, ftpServAD, ftpServHID, CID);
//cerr<<ftpServAD<<"\t"<<ftpServHID<<"\t"<<CID<<endl;

	if (SIDToProfile[remoteSID][CID].fetchState == BLANK) {
		pthread_mutex_lock(&profileLock);
		SIDToProfile[remoteSID][CID].fetchState = PENDING;
		pthread_mutex_unlock(&profileLock);
	}
cerr<<CID<<" fetch state: "<<SIDToProfile[remoteSID][CID].fetchState<<endl;
	free(CID);
	return;
}

// TODO: scheduling 
// TODO: read the list from data structure shared with handlers 
// control plane: figure out the right CIDs to prefetch, change the prefetch state from BLANK to PENDING to READY
void *prefetchData(void *) 
{
	thread_c++;
	int thread_id = thread_c;
cerr<<"Thread id "<<thread_id<<": "<<"is launched\n\n\n\n";
//cerr<<"Current "<<getAD()<<endl;

	// TODO: consider bootstrap for each individual SID
	bool bootstrap = true;
	bool finish = false;

	int netPrefetchSock = registerPrefetchService(getPrefetchServiceName(), myAD, myHID, prefetchAD, prefetchHID); 
	if (netPrefetchSock == -1) {
		netPrefetchOn = false;
	}

	// TODO: handle the case of no in-net prefetching service
	while (1) {
		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {
			if ((*I).second.size() > 0) {

				currSSID = getSSID();

				if (lastSSID != currSSID) {
cerr<<"Thread id "<<thread_id<<": "<<"Network changed, create another thread to continute\n";
					getNewAD(myAD);
					lastSSID = currSSID;
					//ssidChange = true;			
					pthread_t thread_prefetchDataNew; 
					pthread_create(&thread_prefetchDataNew, NULL, prefetchData, NULL);
					finish = true;
				}

				char cmd[XIA_MAX_BUF];
				char reply[XIA_MAX_BUF];
				int n = -1;
				memset(cmd, '\0', strlen(cmd));
				memset(reply, '\0', strlen(reply));
				
				string SID = (*I).first;
//cerr<<"SID: "<<SID<<"chunks to be pulled from the queue\n";				
				vector<string> CIDs_prefetching;

				int s = -1;
				// search for the first CID and the list to prefetch 
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
					// start from the first CID not yet prefetched if it is the first time to prefetch 
					if (bootstrap) {
						if (SIDToProfile[SID][(*I).second[i]].prefetchState == BLANK) {
							s = (int)i;
							bootstrap = false;
							break;
						}
					}
					// start from the first CID not yet fetched 
					else {
						if (SIDToProfile[SID][(*I).second[i]].fetchState != READY) {
							s = (int)i;
							break;
						}
					}
				}
				// all the chunks are prefetched
				if (s == -1) {
					break;
				}
				int e = s + SIDToWindow[SID];
				if (e > (int)(*I).second.size()) {
					e = (int)(*I).second.size();
				}
//cerr<<s<<"\t"<<e<<endl;
				// only update the state of the chunks not prefetched yet 
				for (int i = s; i < e; i++) {
					if (SIDToProfile[SID][(*I).second[i]].prefetchState == BLANK) {
cerr<<(*I).second[i]<<endl;
						CIDs_prefetching.push_back((*I).second[i]);
					}
				}
				// send the prefetching list to the prefetch service and receive message back one by one and update prefetch state
				if (CIDs_prefetching.size() > 0) {
					pthread_mutex_lock(&profileLock);
					sprintf(cmd, "prefetch");
					for (unsigned i = 0; i < CIDs_prefetching.size(); i++) {
						SIDToProfile[SID][CIDs_prefetching[i]].prefetchState = PENDING;								
						SIDToProfile[SID][CIDs_prefetching[i]].fetchAD = prefetchAD;
						SIDToProfile[SID][CIDs_prefetching[i]].fetchAD = prefetchHID;						
						strcat(cmd, " ");
						strcat(cmd, string2char(CIDs_prefetching[i]));
					}
					pthread_mutex_unlock(&profileLock);				
							
					sendStreamCmd(netPrefetchSock, cmd);
					// receive chunk ready msg one by one
					for (unsigned i = 0; i < CIDs_prefetching.size(); i++) {
						if ((n = Xrecv(netPrefetchSock, reply, sizeof(reply), 0)) < 0) {
							Xclose(netPrefetchSock);
							die(-1, "Unable to communicate with the server\n");
						}
cerr<<reply<<endl;
						// update "ready" to prefetch state
						if (strncmp(reply, "ready", 5) == 0) {
							vector<string> prefetched_info = strVector(reply+6);
							if (CIDs_prefetching[i] == prefetched_info[0]) {
								pthread_mutex_lock(&profileLock);								
								SIDToProfile[SID][CIDs_prefetching[i]].prefetchStartTimestamp = string2long(prefetched_info[1]);							
								SIDToProfile[SID][CIDs_prefetching[i]].prefetchFinishTimestamp = string2long(prefetched_info[2]);
								SIDToProfile[SID][CIDs_prefetching[i]].prefetchState = READY;
								pthread_mutex_unlock(&profileLock);
							}
						}
					}
				}
			}
		}
		// finish the last batch prefetching task after changing the network attachment and exit the thread
		if (finish) {
cerr<<"Thread id "<<thread_id<<": "<<"is finished\n\n\n\n";
			// TODO: close the socket
			pthread_exit(NULL);
		}
		usleep(SCAN_DELAY_MSEC*1000);
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
				vector<string> CIDs_fetching;
				// find the CID to fetch behalf of the xftp client
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
					if (SIDToProfile[SID][(*I).second[i]].fetchState == PENDING && SIDToProfile[SID][(*I).second[i]].prefetchState == READY) {
//cerr<<(*I).second[i]<<"\t fetch state: PENDING state\t prefetch state: READY\n"; 
						CIDs_fetching.push_back((*I).second[i]);
					}
				}	
				if (CIDs_fetching.size() > 0) {			
					int chunkSock;

					if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
						die(-1, "unable to create chunk socket\n");

					// TODO: check the size is smaller than the max chunk to fetch at one time
					ChunkStatus cs[CIDs_fetching.size()];
					int status;
					int n = CIDs_fetching.size();

					for (unsigned int i = 0; i < CIDs_fetching.size(); i++) {
						char *dag = (char *)malloc(512);
						// construct the dag with access network's AD and HID
						sprintf(dag, "RE ( %s %s ) CID:%s", string2char(SIDToProfile[SID][CIDs_fetching[i]].fetchAD), string2char(SIDToProfile[SID][CIDs_fetching[i]].fetchHID), string2char(CIDs_fetching[i]));
//cerr<<dag<<endl;
						cs[i].cidLen = strlen(dag);
						cs[i].cid = dag; // cs[i].cid is a DAG, not just the CID
						SIDToProfile[SID][CIDs_fetching[i]].fetchStartTimestamp = now_msec();
					}

					unsigned ctr = 0;

					while (1) {
						if (ctr % REREQUEST == 0) {
							// bring the list of chunks local
							say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
							if (XrequestChunks(chunkSock, cs, n) < 0) {
								say("unable to request chunks\n");
								pthread_exit(NULL); // TODO: check again
							}
							//say("checking chunk status\n");
						}
						ctr++;

						status = XgetChunkStatuses(chunkSock, cs, n);

						if (status == READY_TO_READ) {
							pthread_mutex_lock(&profileLock);	
							for (unsigned int i = 0; i < CIDs_fetching.size(); i++) {
								SIDToProfile[SID][CIDs_fetching[i]].fetchFinishTimestamp = now_msec();								
								SIDToProfile[SID][CIDs_fetching[i]].fetchState = READY;
//cerr<<CIDs[i]<<"\t fetch state: ready\n";
							}
							pthread_mutex_unlock(&profileLock);		
							break;
						}
						else if (status & WAITING_FOR_CHUNK) {
							//say("waiting... one or more chunks aren't ready yet\n");
						}
						else if (status & INVALID_HASH) 
							die(-1, "one or more chunks has an invalid hash");
						else if (status & REQUEST_FAILED)
							die(-1, "no chunks found\n");
						else if (status < 0) 
							die(-1, "error getting chunk status\n"); 						
						else 
							say("unexpected result\n");

						usleep(CHUNK_REQUEST_DELAY_MSEC*1000); 
					}
				}
				// TODO: timeout the chunks by free(cs[i].cid); cs[j].cid = NULL; cs[j].cidLen = 0;			
			}
		} 
	}
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
		// registration msg from xftp client: reg CID1, CID2, ... CIDn
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
				usleep(SCAN_DELAY_MSEC*1000);
			}
		}
		// TODO: to replace with the following with a special socket added later:
		/*
		struct pollfd pfds[2];
		pfds[0].fd = sock;
		pfds[0].events = POLLIN;
		if ((rc = Xpoll(pfds, 1, 5000)) <= 0) {
			die(-5, "Poll returned %d\n", rc);
		}	
		*/			
		usleep(LOOP_DELAY_MSEC*1000);			
	}
	pthread_exit(NULL);
}

void *windowPred(void *) 
{
	long fetch_latency;
	long prefetch_latency;
	while (1) {
		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {
			string SID = (*I).first;
			vector<string> CIDs;
			for (unsigned int i = 0; i < (*I).second.size(); i++) {
				if (SIDToProfile[SID][(*I).second[i]].fetchState == READY) {
					CIDs.push_back((*I).second[i]);
				}
			}
			fetch_latency = 0;
			prefetch_latency = 0;
			if (CIDs.size() > 0 && CIDs.size() <= PREFETCH_WIN_RECENT_NUM) {
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
					fetch_latency += SIDToProfile[SID][CIDs[i]].fetchFinishTimestamp - SIDToProfile[SID][CIDs[i]].fetchStartTimestamp;
					prefetch_latency += SIDToProfile[SID][CIDs[i]].prefetchFinishTimestamp - SIDToProfile[SID][CIDs[i]].prefetchStartTimestamp;					
				}
				pthread_mutex_lock(&windowLock);
				if (prefetch_latency > fetch_latency) {
					SIDToWindow[SID] += 1;
				}
				else {
					if (SIDToWindow[remoteSID] > 1) {
						SIDToWindow[SID] -= 1;					
					}
				}
				pthread_mutex_unlock(&windowLock);
			}
			else if (CIDs.size() > PREFETCH_WIN_RECENT_NUM) {
				for (unsigned int i = (*I).second.size() - 1; i > (*I).second.size() - i - PREFETCH_WIN_RECENT_NUM; i--) {
					fetch_latency += SIDToProfile[SID][CIDs[i]].fetchFinishTimestamp - SIDToProfile[SID][CIDs[i]].fetchStartTimestamp;
					prefetch_latency += SIDToProfile[SID][CIDs[i]].prefetchFinishTimestamp - SIDToProfile[SID][CIDs[i]].prefetchStartTimestamp;					
				}
				pthread_mutex_lock(&windowLock);
				if (prefetch_latency > fetch_latency) {
					SIDToWindow[SID] += 1;
				}
				else {
					if (SIDToWindow[remoteSID] > 1) {
						SIDToWindow[SID] -= 1;					
					}
				}
				pthread_mutex_unlock(&windowLock);
			}
		}
		sleep(PREFETCH_WIN_PRED_DELAY_SEC);
	}
	pthread_exit(NULL);
}

// TODO: if the SID's all CIDs have READY fetch state, then remove
void *profileMgt(void *) 
{
	while (1) {
		for (map<string, long>::iterator I = SIDToTime.begin(); I != SIDToTime.end(); ++I) {
			if (now_msec() - (*I).second >= PURGE_DELAY_SEC*1000) {
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
		sleep(MGT_DELAY_SEC);
	}
	pthread_exit(NULL);	
}

int main() 
{
	pthread_t thread_windowPred, thread_mgt, thread_fetchData, thread_prefetchData; // TODO: thread_netMon
	//pthread_create(&thread_netMon, NULL, netMon, NULL);	// TODO: improve the netMon function
	pthread_create(&thread_windowPred, NULL, windowPred, NULL);	// TODO: improve the netMon function
	pthread_create(&thread_mgt, NULL, profileMgt, NULL); // dequeue, prefetch and update profile	
	pthread_create(&thread_fetchData, NULL, fetchData, NULL);		
	pthread_create(&thread_prefetchData, NULL, prefetchData, NULL);	

	ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID); // get ftpServAD and ftpServHID for building chunk request
	// TODO: handle when SSID is null
	lastSSID = execSystem(GETSSID_CMD);
	currSSID = execSystem(GETSSID_CMD);
	strcpy(currAD, myAD);

	localPrefetchSock = registerStreamReceiver(getPrefetchManagerName(), myAD, myHID, my4ID); // communicate with client app
	blockListener((void *)&localPrefetchSock, clientCmd);

	return 0;	
}

/*
cerr<<"\nPrint profile table in reg_handler\n";
	for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
		for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
		}
	}	
*/