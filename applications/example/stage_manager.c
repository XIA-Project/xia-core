#include "stage_utils.h"

using namespace std;

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char remoteAD[MAX_XID_SIZE];
char remoteHID[MAX_XID_SIZE];
char remoteSID[MAX_XID_SIZE];

char stageAD[MAX_XID_SIZE];
char stageHID[MAX_XID_SIZE];

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

char netHID[MAX_XID_SIZE];

char currAD[MAX_XID_SIZE];

char fetchAD[MAX_XID_SIZE];
char fetchHID[MAX_XID_SIZE];

string lastSSID, currSSID;

struct chunkProfile {
	int fetchState;
	int stageState;	
	string fetchAD;
	string fetchHID;	
	long fetchStartTimestamp;
	long fetchFinishTimestamp;	
	long stageStartTimestamp;	
	long stageFinishTimestamp;	
};

map<string, vector<string> > SIDToCIDs;
map<string, map<string, chunkProfile> > SIDToProfile;
map<string, unsigned int> SIDToWindow; // staging window
map<string, long> SIDToTime; // store the timestamp last seen

int stageSock, ftpSock;

// chenren
bool netStageOn = true;

int thread_c = 0;

pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t windowLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t timeLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cidVecLock = PTHREAD_MUTEX_INITIALIZER;

// TODO: this is not easy to manage in a centralized manner because we can have multiple threads for staging data due to mobility and we should have a pair of mutex and cond for each thread
pthread_mutex_t stageLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  stageCond = PTHREAD_COND_INITIALIZER;

pthread_mutex_t fetchLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  fetchCond = PTHREAD_COND_INITIALIZER;

bool netChange, netChangeACK;

// TODO: mem leak; window pred alg; int netMonSock;
void regHandler(int sock, char *cmd)
{
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); 
//cerr<<"Peer SID: "<<remoteSID<<endl; 
cerr<<cmd<<endl;

	if (strncmp(cmd, "reg cont", 8) == 0) {
cerr<<"Receiving partial chunk list"<<endl;
		char cmd_arr[strlen(cmd+9)];
		strcpy(cmd_arr, cmd+9);
		char *cid;
		pthread_mutex_lock(&cidVecLock);		
		SIDToCIDs[remoteSID].push_back(strtok(cmd_arr, " "));	
		while ((cid = strtok(NULL, " ")) != NULL) {
			SIDToCIDs[remoteSID].push_back(cid);
cerr<<cid<<endl;
		}
		pthread_mutex_unlock(&cidVecLock);			
		return;
	}

//	vector<string> CIDs = strVector(cmd);
	else if (strncmp(cmd, "reg done", 8) == 0) {
		pthread_mutex_lock(&windowLock);	
		SIDToWindow[remoteSID] = STAGE_WIN_INIT;
		pthread_mutex_unlock(&windowLock);

		pthread_mutex_lock(&timeLock);	
		SIDToTime[remoteSID] = now_msec();
		pthread_mutex_unlock(&timeLock);	

		map<string, chunkProfile> profile;
		for (unsigned int i = 0; i < SIDToCIDs[remoteSID].size(); i++) {
			chunkProfile cp;
			cp.fetchState = BLANK;
			cp.stageState = BLANK;
			cp.fetchAD = "";
			cp.fetchHID = "";
			cp.fetchStartTimestamp = 0;
			cp.fetchFinishTimestamp = 0;	
			cp.stageStartTimestamp = 0;	
			cp.stageFinishTimestamp = 0;	
			profile[SIDToCIDs[remoteSID][i]] = cp;			


//cerr<<i<<"\t"<<SIDToCIDs[remoteSID][i]<<endl;
		}	
		pthread_mutex_lock(&profileLock);
		SIDToProfile[remoteSID] = profile;
		pthread_mutex_unlock(&profileLock);
cerr<<"Quit the profileLock and finish reg"<<endl;
		//pthread_mutex_lock(&stageLock); 
		//pthread_cond_signal(&stageCond);
		//pthread_mutex_unlock(&stageLock);	
		return;
	}
}

void delegationHandler(int sock, char *cmd) 
{
cerr<<cmd<<endl; 
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); 
	size_t cidLen;
	char *CID = (char *)malloc(512); 
	// TODO: right now it's hacky, need to fix the way reading XIDs when including fallback DAG
	sscanf(cmd, "%ld RE ( %s %s ) CID:%s", &cidLen, ftpServAD, ftpServHID, CID);

	if (SIDToProfile[remoteSID][CID].fetchState == BLANK) {
		pthread_mutex_lock(&profileLock);
		SIDToProfile[remoteSID][CID].fetchState = PENDING;
cerr<<"Change from BLANK to PENDING\n";		
		pthread_mutex_unlock(&profileLock);
	}
//cerr<<CID<<" fetch state: "<<SIDToProfile[remoteSID][CID].fetchState<<endl;
	free(CID);

	pthread_mutex_lock(&timeLock);	
	SIDToTime[remoteSID] = now_msec();
	pthread_mutex_unlock(&timeLock);	

//	pthread_mutex_lock(&stageLock); 
//	pthread_cond_signal(&stageCond);
//	pthread_mutex_unlock(&stageLock);	

//	pthread_mutex_lock(&fetchLock); 
//	pthread_cond_signal(&fetchCond);
//	pthread_mutex_unlock(&fetchLock);	

	return;
}

void *clientCmd(void *socketid) 
{
	char cmd[XIA_MAXBUF];
	int sock = *((int*)socketid);
	int n = -1;

	while (1) {
		memset(cmd, '\0', XIA_MAXBUF);
		if ((n = Xrecv(sock, cmd, sizeof(cmd), 0)) < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		} 
		// registration msg from xftp client: reg CID1, CID2, ... CIDn
		if (strncmp(cmd, "reg", 3) == 0) {
			say("Receive a chunk list registration message\n");
			regHandler(sock, cmd);
		}
		// chunk request from xftp client: fetch CID
		else if (strncmp(cmd, "fetch", 5) == 0) {
			say("Receive a chunk request\n");
			delegationHandler(sock, cmd+6);
		}
		usleep(10000);
	}
	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

// WORKING version
// TODO: scheduling 
// control plane: figure out the right CIDs to stage, change the stage state from BLANK to PENDING to READY
void *stageData(void *) 
{
	thread_c++;
	int thread_id = thread_c;
cerr<<"Thread id "<<thread_id<<": "<<"Is launched\n";
cerr<<"Current "<<getAD()<<endl;

	// TODO: consider bootstrap for each individual SID
	bool bootstrap = true;
	bool finish = false;

	int netStageSock = registerStageService(getStageServiceName(), myAD, myHID, stageAD, stageHID); 

	if (netStageSock == -1) {
		netStageOn = false;
	}

	// TODO: need to handle the case that new SID joins dynamically
	// TODO: handle no in-net prefetching service
	while (1) {
		//pthread_mutex_lock(&stageLock); 
		//pthread_cond_wait(&stageCond, &stageLock);			
		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {
			if ((*I).second.size() > 0) {//} && SIDToProfile[(*I).first][(*I).second[(*I).second.size()-1]].fetchState != READY) {
				// network change hander
				currSSID = getSSID();

				if (lastSSID != currSSID) {
cerr<<"Thread id "<<thread_id<<": "<<"Network changed, create another thread to continute\n";
					getNewAD(myAD);
					lastSSID = currSSID;
					//ssidChange = true;			
					pthread_t thread_stageDataNew; 
					pthread_create(&thread_stageDataNew, NULL, stageData, NULL);
					finish = true; // need to finish
				}

				char cmd[XIA_MAX_BUF];
				char reply[XIA_MAX_BUF];
				int n = -1;
				memset(cmd, '\0', strlen(cmd));
				memset(reply, '\0', strlen(reply));
				
				string SID = (*I).first;
//cerr<<"SID: "<<SID<<"chunks to be pulled from the queue\n";				
				vector<string> CIDs;
				// find the right list to fetch
				int s = -1;
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
					// start from the first CID not yet prefetched if it is the first time to prefetch 
					if (bootstrap) {
						if (SIDToProfile[SID][(*I).second[i]].stageState == BLANK) {
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
				pthread_mutex_lock(&profileLock);	
				// only update the state of the chunks not prefetched yet 
				for (int i = s; i < e; i++) {
					if (SIDToProfile[SID][(*I).second[i]].stageState == BLANK) {
cerr<<(*I).second[i]<<endl;						
						CIDs.push_back((*I).second[i]);
						SIDToProfile[SID][(*I).second[i]].stageState = PENDING;
						SIDToProfile[SID][(*I).second[i]].fetchAD = stageAD;
						SIDToProfile[SID][(*I).second[i]].fetchAD = stageHID;
					}
				}
				pthread_mutex_unlock(&profileLock);

				// send the prefetching list to the prefetch service and receive message back and update prefetch state
				// TODO: non-block fasion 
				if (CIDs.size() > 0) {
					sprintf(cmd, "stage");
					for (unsigned i = 0; i < CIDs.size(); i++) {
						strcat(cmd, " ");
						strcat(cmd, string2char(CIDs[i]));
					}				
					sendStreamCmd(netStageSock, cmd);

					if ((n = Xrecv(netStageSock, reply, sizeof(reply), 0)) < 0) {
						Xclose(netStageSock);
						die(-1, "Unable to communicate with the server\n");
					}
	cerr<<reply<<endl;
					// update "ready" to prefetch state
					if (strncmp(reply, "ready", 5) == 0) {
						vector<string> CIDs_staging = strVector(reply+6);
						pthread_mutex_lock(&profileLock);							
						for (unsigned i = 0; i < CIDs_staging.size(); i++) {
							SIDToProfile[SID][CIDs_staging[i]].stageState = READY;
						}
						pthread_mutex_unlock(&profileLock);
					}	
				}
			}
		}
		// after looping all the SIDs
		if (finish) {
			pthread_exit(NULL);
		}
		usleep(10000);
		//pthread_mutex_unlock(&fetchLock); 		
	}
	pthread_exit(NULL);
}

// WORKING version
// data plane of prefetching: find the pending chunk, construct the dag and send the pending chunk request, 
void *fetchData(void *) 
{
	int chunkSock;

	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");

	int status = -1;

	while (1) {
		//pthread_mutex_lock(&fetchLock); 
		//pthread_cond_wait(&fetchCond, &fetchLock);			
		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {
			if ((*I).second.size() > 0) {// && SIDToProfile[(*I).first][(*I).second[(*I).second.size()-1]].fetchState != READY) {
				string SID = (*I).first;
//cerr<<"SID: "<<SID<<"chunks to be pulled from the queue\n";				
				vector<string> CIDs_fetching;
				// find the CID to fetch behalf of the xftp client
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
					if (SIDToProfile[SID][(*I).second[i]].fetchState == PENDING && SIDToProfile[SID][(*I).second[i]].stageState == READY) {
cerr<<(*I).second[i]<<"\t fetch state: PENDING state\t prefetch state: READY\n"; 
						CIDs_fetching.push_back((*I).second[i]);
					}
				}	
				if (CIDs_fetching.size() > 0) {			
					// TODO: check the size is smaller than the max chunk to fetch at one time
					ChunkStatus cs[CIDs_fetching.size()];
					int n = CIDs_fetching.size();

					for (unsigned int i = 0; i < CIDs_fetching.size(); i++) {
						char *dag = (char *)malloc(512);
						sprintf(dag, "RE ( %s %s ) CID:%s", string2char(SIDToProfile[SID][CIDs_fetching[i]].fetchAD), string2char(SIDToProfile[SID][CIDs_fetching[i]].fetchHID), string2char(CIDs_fetching[i]));
	cerr<<dag<<endl;					
						cs[i].cidLen = strlen(dag);
						cs[i].cid = dag; // cs[i].cid is a DAG, not just the CID
						pthread_mutex_lock(&profileLock);	
						SIDToProfile[SID][CIDs_fetching[i]].fetchStartTimestamp = now_msec();
						pthread_mutex_unlock(&profileLock);	
						//free(dag); chenren: why freeze the program!!!!! see error msg below
						// [libprotobuf ERROR google/protobuf/wire_format.cc:1053] String field contains invalid UTF-8 data when parsing a protocol buffer. Use the 'bytes' type if you intend to send raw bytes.
						//click: ../include/click/vector.hh:59: const T& Vector<T>::operator[](Vector<T>::size_type) const [with T = XIAPath::Node; Vector<T>::size_type = int]: Assertion `(unsigned) i < (unsigned) _n' failed.
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
							//say("checking chunk status\n");
						}
						ctr++;

						status = XgetChunkStatuses(chunkSock, cs, n);

						if (status == READY_TO_READ) {
							pthread_mutex_lock(&profileLock);	
							for (unsigned int i = 0; i < CIDs_fetching.size(); i++) {
								SIDToProfile[SID][CIDs_fetching[i]].fetchState = READY;
cerr<<CIDs_fetching[i]<<"\t fetch state: ready\n";
							}
							pthread_mutex_unlock(&profileLock);		
							break;
						}
						else if (status < 0) 
							die(-1, "error getting chunk status\n"); 
						else if (status & WAITING_FOR_CHUNK) {
							//say("waiting... one or more chunks aren't ready yet\n");
						}
						else if (status & INVALID_HASH) 
							die(-1, "one or more chunks has an invalid hash");
						else if (status & REQUEST_FAILED)
							die(-1, "no chunks found\n");
						else 
							say("unexpected result\n");

						usleep(100000); 
					}
				}
				// TODO: timeout the chunks by free(cs[i].cid); cs[j].cid = NULL; cs[j].cidLen = 0;			
			}
		} 
		usleep(LOOP_DELAY_MSEC*1000);		

		//pthread_mutex_unlock(&fetchLock); 		
	}
	// 	big loop, SIDToProfile[remoteSID][CID].fetchState = PENDING and prefetch state == done, construct dag, fetch data, update
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

/*
void *winPred(void *) 
{
	long fetch_latency;
	long stage_latency;
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
			stage_latency = 0;
			if (CIDs.size() > 0 && CIDs.size() <= STAGE_WIN_RECENT_NUM) {
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
					fetch_latency += SIDToProfile[SID][CIDs[i]].fetchFinishTimestamp - SIDToProfile[SID][CIDs[i]].fetchStartTimestamp;
					stage_latency += SIDToProfile[SID][CIDs[i]].stageFinishTimestamp - SIDToProfile[SID][CIDs[i]].stageStartTimestamp;					
				}
				pthread_mutex_lock(&windowLock);
				if (stage_latency > fetch_latency) {
					SIDToWindow[SID] += 1;
				}
				else {
					if (SIDToWindow[SID] > 1) {
						SIDToWindow[SID] -= 1;					
					}
				}
				pthread_mutex_unlock(&windowLock);
			}
			else if (CIDs.size() > STAGE_WIN_RECENT_NUM) {
				for (unsigned int i = (*I).second.size() - 1; i > (*I).second.size() - i - STAGE_WIN_RECENT_NUM; i--) {
					fetch_latency += SIDToProfile[SID][CIDs[i]].fetchFinishTimestamp - SIDToProfile[SID][CIDs[i]].fetchStartTimestamp;
					stage_latency += SIDToProfile[SID][CIDs[i]].stageFinishTimestamp - SIDToProfile[SID][CIDs[i]].stageStartTimestamp;					
				}
				pthread_mutex_lock(&windowLock);
				if (stage_latency > fetch_latency) {
					SIDToWindow[SID] += 1;
				}
				else {
					if (SIDToWindow[SID] > 1) {
						SIDToWindow[SID] -= 1;					
					}
				}
				pthread_mutex_unlock(&windowLock);
			}
		}
		sleep(STAGE_WIN_PRED_DELAY_SEC);
	}
	pthread_exit(NULL);
}

*/

// TODO: if the SID's all CIDs have READY fetch state, then remove
void *profileMgt(void *) 
{
	while (1) {
		/*
		for (map<string, long>::iterator I = SIDToTime.begin(); I != SIDToTime.end(); ++I) {
			if (now_msec() - (*I).second >= PURGE_DELAY_SEC*1000) {				
				string SID = (*I).first;
cerr<<"Deleting profile of "<<SID<<endl;
				pthread_mutex_lock(&profileLock);
				SIDToProfile.erase(SID);
				pthread_mutex_unlock(&profileLock);
cerr<<"Deleting CIDs of "<<SID<<endl;
				pthread_mutex_lock(&cidVecLock);
				SIDToCIDs.erase(SID);
				pthread_mutex_unlock(&cidVecLock);
cerr<<"Deleting window of "<<SID<<endl;				
				pthread_mutex_lock(&windowLock);
				SIDToWindow.erase(SID);
				pthread_mutex_unlock(&windowLock);
cerr<<"Deleting time of "<<SID<<endl;				
				pthread_mutex_lock(&timeLock);
				SIDToTime.erase(SID);
				pthread_mutex_unlock(&timeLock);
			}

		}	
		*/
		sleep(MGT_DELAY_SEC);
	}
	pthread_exit(NULL);	
}

int main() 
{
	// chenren
/*	
	pthread_t thread_winPred, thread_mgt, thread_fetchData, thread_stageData; // TODO: thread_netMon
	//pthread_create(&thread_netMon, NULL, netMon, NULL);	// TODO: improve the netMon function
	//pthread_create(&thread_winPred, NULL, winPred, NULL);	// TODO: improve the netMon function
	pthread_create(&thread_mgt, NULL, profileMgt, NULL); // dequeue, stage and update profile	
	pthread_create(&thread_fetchData, NULL, fetchData, NULL);		
	pthread_create(&thread_stageData, NULL, stageData, NULL);	
*/

	pthread_t thread_winPred, thread_mgt, thread_netMon, thread_fetchData, thread_stageData; // TODO: thread_netMon
	pthread_create(&thread_netMon, NULL, netMon, NULL);	// TODO: improve the netMon function
	//pthread_create(&thread_winPred, NULL, winPred, NULL);	// TODO: improve the netMon function	
	pthread_create(&thread_mgt, NULL, profileMgt, NULL); // dequeue, prefetch and update profile	
	pthread_create(&thread_fetchData, NULL, fetchData, NULL);		
	pthread_create(&thread_stageData, NULL, stageData, NULL);	

	ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID); // get ftpServAD and ftpServHID for building chunk request
	// TODO: handle when SSID is null
	lastSSID = execSystem(GETSSID_CMD);
	currSSID = execSystem(GETSSID_CMD);
	strcpy(currAD, myAD);

	stageSock = registerStreamReceiver(getStageManagerName(), myAD, myHID, my4ID); // communicate with client app	
	blockListener((void *)&stageSock, clientCmd);

	return 0;	
}

/*
// TODO: scheduling 
// control plane: figure out the right CIDs to stage, change the stage state from BLANK to PENDING to READY
void *stageData(void *) 
{
	thread_c++;
	int thread_id = thread_c;
cerr<<"Thread id "<<thread_id<<": "<<"is launched\n";
//cerr<<"Current "<<getAD()<<endl;

	// TODO: consider bootstrap for each individual SID
	bool bootstrap = true;
	bool finish = false;

	int netStageSock = registerStageService(getStageServiceName(), myAD, myHID, stageAD, stageHID); 
	if (netStageSock == -1) {
		netStageOn = false;
	}
	// TODO: handle the case of no in-net staging service
	while (1) {
		//pthread_mutex_lock(&stageLock); 
		//pthread_cond_wait(&stageCond, &stageLock);			
		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {
			if ((*I).second.size() > 0){//} && SIDToProfile[(*I).first][(*I).second[(*I).second.size()-1]].fetchState != READY) {
//cerr<<"stageData is still running\n";
				currSSID = getSSID();

				if (lastSSID != currSSID) {
cerr<<"Thread id "<<thread_id<<": "<<"Network changed, create another thread to continute\n";
					getNewAD(myAD);
					lastSSID = currSSID;
					//ssidChange = true;			
					pthread_t thread_stageDataNew; 
					pthread_create(&thread_stageDataNew, NULL, stageData, NULL);
					finish = true;
				}

				char cmd[XIA_MAX_BUF];
				memset(cmd, '\0', strlen(cmd));
				char reply[XIA_MAX_BUF];
				int n = -1;
				
				string SID = (*I).first;
//cerr<<"SID: "<<SID<<"chunks to be pulled from the queue\n";				
				vector<string> CIDs_staging;

				int s = -1;
				// search for the first CID and the list to stage 
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
					// start from the first CID not yet staged if it is the first time to stage 
					if (bootstrap) {
						if (SIDToProfile[SID][(*I).second[i]].stageState == BLANK) {
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
				// all the chunks are staged
				if (s == -1) {
					break;
				}
				int e = s + SIDToWindow[SID];
				if (e > (int)(*I).second.size()) {
					e = (int)(*I).second.size();
				}
//cerr<<s<<"\t"<<e<<endl;
				// only update the state of the chunks not staged yet 
				for (int i = s; i < e; i++) {
					if (SIDToProfile[SID][(*I).second[i]].stageState == BLANK) {
cerr<<(*I).second[i]<<endl;
						CIDs_staging.push_back((*I).second[i]);
					}
				}
			
				// send the staging list to the stage service and receive message back one by one and update stage state
				if (CIDs_staging.size() > 0) {
					pthread_mutex_lock(&profileLock);
					// chenren
					//sprintf(cmd, "stage");
					sprintf(cmd, "prefetch");
					for (unsigned i = 0; i < CIDs_staging.size(); i++) {
						SIDToProfile[SID][CIDs_staging[i]].stageState = PENDING;								
						SIDToProfile[SID][CIDs_staging[i]].fetchAD = stageAD;
						SIDToProfile[SID][CIDs_staging[i]].fetchHID = stageHID;						
						strcat(cmd, " ");
						strcat(cmd, string2char(CIDs_staging[i]));
					}
					pthread_mutex_unlock(&profileLock);

					sendStreamCmd(netStageSock, cmd);

					if ((n = Xrecv(netStageSock, reply, sizeof(reply), 0)) < 0) {
						Xclose(netStageSock);
						die(-1, "Unable to communicate with the server\n");
					}
	cerr<<reply<<endl;
					// update "ready" to prefetch state
					if (strncmp(reply, "ready", 5) == 0) {
						vector<string> CIDs_staging = strVector(reply+6);
						pthread_mutex_lock(&profileLock);
						for (unsigned i = 0; i < CIDs_staging.size(); i++) {
							SIDToProfile[SID][CIDs_staging[i]].stageState = READY;
						}
						pthread_mutex_unlock(&profileLock);									
					}	

//////// 
					// receive chunk ready msg one by one
					for (unsigned i = 0; i < CIDs_staging.size(); i++) {
						memset(reply, '\0', strlen(reply));
						if ((n = Xrecv(netStageSock, reply, sizeof(reply), 0)) < 0) {
							Xclose(netStageSock);
							die(-1, "Unable to communicate with the server\n");
						}
cerr<<reply<<endl;
						// update "ready" to stage state
						if (strncmp(reply, "ready", 5) == 0) {
							vector<string> staged_info = strVector(reply+6);
							if (CIDs_staging[i] == staged_info[0]) {
								pthread_mutex_lock(&profileLock);								
								SIDToProfile[SID][CIDs_staging[i]].stageStartTimestamp = string2long(staged_info[1]);							
								SIDToProfile[SID][CIDs_staging[i]].stageFinishTimestamp = string2long(staged_info[2]);
								SIDToProfile[SID][CIDs_staging[i]].stageState = READY;
								pthread_mutex_unlock(&profileLock);
							}
						}
					}
////////		
				}
			}
		}
		// finish the last batch staging task after changing the network attachment and exit the thread
		if (finish) {
cerr<<"Thread id "<<thread_id<<": "<<"is finished\n\n\n\n";
			// TODO: close the socket
			pthread_exit(NULL);
		}
		usleep(LOOP_DELAY_MSEC*1000);
		//pthread_mutex_unlock(&fetchLock); 		
	}
	pthread_exit(NULL);
}

// data plane of staging: find the chunk with pending state for fetch and ready for stage, construct the dag and send the chunk request, 
void *fetchData(void *) 
{
	int chunkSock;
	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0) {
		die(-1, "unable to create chunk socket\n");
	}

	int status = -1;

	while (1) {
		//pthread_mutex_lock(&fetchLock); 
		//pthread_cond_wait(&fetchCond, &fetchLock);		
		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {
			if ((*I).second.size() > 0) {// && SIDToProfile[(*I).first][(*I).second[(*I).second.size()-1]].fetchState != READY) {
//cerr<<"fetchData is still running\n";				
				string SID = (*I).first;
//cerr<<"SID: "<<SID<<"chunks to be pulled from the queue\n";				
				vector<string> CIDs_fetching;
				// find the CID to fetch behalf of the xftp client
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
					if (SIDToProfile[SID][(*I).second[i]].fetchState == PENDING && SIDToProfile[SID][(*I).second[i]].stageState == READY) {
//cerr<<(*I).second[i]<<"\t fetch state: PENDING state\t stage state: READY\n"; 
						CIDs_fetching.push_back((*I).second[i]);
					}
				}	
				if (CIDs_fetching.size() > 0) {
cerr<<CIDs_fetching.size()<<endl;				
					// TODO: check the size is smaller than the max chunk to fetch at one time
					ChunkStatus cs[CIDs_fetching.size()];
					int n = CIDs_fetching.size();
cerr<<"OK\n";
					for (unsigned int i = 0; i < CIDs_fetching.size(); i++) {
						char *dag = (char *)malloc(512);
						// construct the dag with access network's AD and HID
						sprintf(dag, "RE ( %s %s ) CID:%s", string2char(SIDToProfile[SID][CIDs_fetching[i]].fetchAD), string2char(SIDToProfile[SID][CIDs_fetching[i]].fetchHID), string2char(CIDs_fetching[i]));
cerr<<dag<<endl;
						cs[i].cidLen = strlen(dag);
						cs[i].cid = dag; // cs[i].cid is a DAG, not just the CID
						SIDToProfile[SID][CIDs_fetching[i]].fetchStartTimestamp = now_msec();
						free(dag);
					}
cerr<<"OK\n";

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
cerr<<"before get status\n";
						status = XgetChunkStatuses(chunkSock, cs, n);
cerr<<"after get status\n";
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
		//pthread_mutex_unlock(&fetchLock); 
		usleep(LOOP_DELAY_MSEC*1000);		
	}
	pthread_exit(NULL);
}





*/


/*
cerr<<"\nPrint profile table in reg_handler\n";
	for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
		for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).stage<<endl;
		}
	}	
*/

/*
	cerr<<"print profile table:\n";
	for (map<string, map<string, chunkProfile> >::iterator I = SIDToProfile.begin(); I != SIDToProfile.end(); ++I) {
		map<string, chunkProfile> temp = I->second;
		for (map<string, chunkProfile>::iterator J = temp.begin(); J != temp.end(); ++J) {
cerr<<(*I).first<<"\t"<<(*J).first<<endl;//<<"\t"<<J->second.fetchState<<endl;
		}
	}
*/


/*
void regHandler(int sock, char *cmd)
{
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); 
//cerr<<"Peer SID: "<<remoteSID<<endl; 
cerr<<cmd<<endl;

	pthread_mutex_lock(&windowLock);	
	SIDToWindow[remoteSID] = STAGE_WIN_INIT; // TODO: to optimize
	pthread_mutex_unlock(&windowLock);

	pthread_mutex_lock(&timeLock);	
	SIDToTime[remoteSID] = now_msec();
	pthread_mutex_unlock(&timeLock);

	vector<string> CIDs = strVector(cmd);
	pthread_mutex_lock(&profileLock);	
	SIDToCIDs[remoteSID] = CIDs;
	for (unsigned int i = 0; i < CIDs.size(); i++) {
		chunkProfile cs;
		cs.fetchState = BLANK;
		cs.stageState = BLANK;
		cs.fetchAD = "";
		cs.fetchHID = "";
		cs.fetchStartTimestamp = 0;
		cs.fetchFinishTimestamp = 0;	
		cs.stageStartTimestamp = 0;	
		cs.stageFinishTimestamp = 0;	
		SIDToProfile[remoteSID][CIDs[i]] = cs;
	}	
	pthread_mutex_unlock(&profileLock);	

cerr<<"Finish reg"<<endl;

	return;
}

// TODO: change from 0 to 1 or intercept if it has been requested, 
void prefetchHandler(int sock, char *cmd) {
cerr<<"Receving Command: "<<cmd<<endl; 
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); 
	size_t cidLen;
	//char *cidDag = (char *)malloc(512);
	char *CID = (char *)malloc(512); // TODO: optimize the size?
	// TODO: right now it's hacky, need to fix the way reading XIDs when including fallback DAG
	sscanf(cmd, "%ld RE ( %s %s ) CID:%s", &cidLen, ftpServAD, ftpServHID, CID);
	//sscanf(cmd, "%ld %s", &cidLen, cidDag);
//cerr<<cidLen<<"\t"<<cidDag<<endl;
//	sscanf(cidDag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, CID);
cerr<<ftpServAD<<"\t"<<ftpServHID<<"\t"<<CID<<endl;

	// TODO: if existed, do we need to update timestamp?
	pthread_mutex_lock(&profileLock);
	if (SIDToProfile[remoteSID][CID].fetchState != READY) {
		SIDToProfile[remoteSID][CID].fetchState = PENDING;
		SIDToProfile[remoteSID][CID].fetchStartTimestamp = now_msec();
	}
	else {
		cerr<<CID<<": It's ready!\n";
	}
	pthread_mutex_unlock(&profileLock);
	free(CID);
	return;
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
*/
