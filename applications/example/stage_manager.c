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

//TODO: better to rename in case of mixing up with the chunkProfile struct in server   --Lwy   1.16
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

int stageSock, ftpSock, clientAcceptSock;

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

pthread_mutex_t StageFetchLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t FetchDelegationLock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t StageControl = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t FetchControl = PTHREAD_MUTEX_INITIALIZER;
bool netChange, netChangeACK;

// TODO: mem leak; window pred alg; int netMonSock;
void regHandler(int sock, char *cmd)
{
	say("In regHandler.\n");
	char send_to_client_[XIA_MAX_BUF];
	memset(send_to_client_, '\0', strlen(send_to_client_));
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
		say("++++++++++++++++++++++++++++++++The remoteSID is %s\n", remoteSID);
		pthread_mutex_unlock(&cidVecLock);
		sprintf(send_to_client_, "Continue to register CID.");
		say("Ready to sent command to client.\n");
		sayHello(sock, send_to_client_);
		say("Successfully sent Continue command to client.\n");
		return;
	}

//	vector<string> CIDs = strVector(cmd);
	else if (strncmp(cmd, "reg done", 8) == 0) {
		say("Receive the reg done command\n");
		pthread_mutex_lock(&windowLock);
		SIDToWindow[remoteSID] = STAGE_WIN_INIT;
		pthread_mutex_unlock(&windowLock);

		pthread_mutex_lock(&timeLock);
		SIDToTime[remoteSID] = now_msec();
		pthread_mutex_unlock(&timeLock);

		map<string, chunkProfile> profile;
        //add lock and unlock(cidVecLock)   --Lwy   1.16
        pthread_mutex_lock(&cidVecLock);
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
			say("Updating profile: %d/%d\n", i + 1, SIDToCIDs[remoteSID].size());
		}
        pthread_mutex_unlock(&cidVecLock);
		pthread_mutex_lock(&profileLock);
		SIDToProfile[remoteSID] = profile;
		pthread_mutex_unlock(&profileLock);
	    //say("Ready to sent command to client.\n");
		sprintf(send_to_client_, "Done profile.");
		sayHello(sock, send_to_client_);
		say("Successfully sent Done profile. command to client.\n");
cerr<<"Quit the profileLock and finish reg"<<endl;
		//pthread_mutex_lock(&stageLock);
		//pthread_cond_signal(&stageCond);
		//pthread_mutex_unlock(&stageLock);
		return;
	}
}

void delegationHandler(int sock, char *cmd)
{
	say("In delegationHandler.\n");
	say("The command is %s\n", cmd);
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID);
	size_t cidLen;
	char *CID = (char *)malloc(512);
	char send_to_client_[XIA_MAX_BUF];
	memset(send_to_client_, '\0', strlen(send_to_client_));
	// TODO: right now it's hacky, need to fix the way reading XIDs when including fallback DAG
	sscanf(cmd, "%ld RE ( %s %s ) CID:%s", &cidLen, ftpServAD, ftpServHID, CID);

    //put the lock and unlock outside the loop  --Lwy   1.16
	pthread_mutex_lock(&profileLock);
	if (SIDToProfile[remoteSID][CID].fetchState == BLANK) {
		//pthread_mutex_lock(&profileLock);
		SIDToProfile[remoteSID][CID].fetchState = PENDING;
cerr<<"CID: "<< CID << " change from BLANK to PENDING\n";
		//pthread_mutex_unlock(&profileLock);
	}
    pthread_mutex_unlock(&profileLock);
//cerr<<CID<<" fetch state: "<<SIDToProfile[remoteSID][CID].fetchState<<endl;
	free(CID);

	pthread_mutex_lock(&timeLock);
	SIDToTime[remoteSID] = now_msec();
	pthread_mutex_unlock(&timeLock);

	sprintf(send_to_client_, "Finish current delegationHandler.");
	//sayHello(sock, send_to_client_);

//	pthread_mutex_lock(&stageLock);
//	pthread_cond_signal(&stageCond);
//	pthread_mutex_unlock(&stageLock);

//	pthread_mutex_lock(&fetchLock);
//	pthread_cond_signal(&fetchCond);
//	pthread_mutex_unlock(&fetchLock);

    //pthread_mutex_lock(&FetchDelegationLock);
	return;
}

void *clientCmd(void *socketid)
{
	char cmd[XIA_MAXBUF];
	int sock = *((int*)socketid);
	int n = -1;
	char client_[XIA_MAX_BUF];
	memset(client_, '\0', strlen(client_));
	sprintf(client_, "clientCmd```````````````````````````````````Ready to receive cmd from client.\n");

    void *stageData(void *);
    void *fetchData(void *);
	while (1) {
		memset(cmd, '\0', XIA_MAXBUF);

		//sayHello(sock, client_);
		if ((n = Xrecv(sock, cmd, sizeof(cmd), 0)) < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		say("clientCmd````````````````````````````````Receive the command that: %s\n", cmd);
		// registration msg from xftp client: reg CID1, CID2, ... CIDn
		if (strncmp(cmd, "reg", 3) == 0) {
			say("Receive a chunk list registration message\n");
			regHandler(sock, cmd);
			pthread_mutex_unlock(&StageControl);
		}
		// chunk request from xftp client: fetch CID
		else if (strncmp(cmd, "fetch", 5) == 0) {
			say("Receive a chunk request\n");
			say("clientCmd````````````````````````````The sock is %d\n", sock);
			delegationHandler(sock, cmd + 6);
			pthread_mutex_unlock(&StageControl);
		}
		usleep(SCAN_DELAY_MSEC*1000);
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
	//say("------------------------------In stageData.\n");
	thread_c++;
	int thread_id = thread_c;
cerr << "Thread id " << thread_id << ": " << "Is launched\n";
cerr << "Current " << getAD() << endl;

	// TODO: consider bootstrap for each individual SID
	bool bootstrap = true;
	bool finish = false;

//netStageSock is used to communicate with stage server.
	int netStageSock = registerStageService(getStageServiceName(), myAD, myHID, stageAD, stageHID);
    say("++++++++++++++++++++++++++++++++++++The current netStageSock is %d\n", netStageSock);
	if (netStageSock == -1) {
		netStageOn = false;
	}

	// TODO: need to handle the case that new SID joins dynamically
	// TODO: handle no in-net staging service
	while (1) {
		pthread_mutex_lock(&StageControl);
		say("************************************In while loop of stageData\n");
		//pthread_mutex_lock(&stageLock);
		//pthread_cond_wait(&stageCond, &stageLock);
		int tmp_cnt = 1;

		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {
			say("Handling %d/%d SIDs.\n", tmp_cnt, SIDToCIDs.size());
			say("Number of CID is %d.\n", (*I).second.size());
			tmp_cnt++;
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
					pthread_mutex_unlock(&StageControl);
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
					// start from the first CID not yet staged if it is the first time to stage
					if (bootstrap) {
						if (SIDToProfile[SID][(*I).second[i]].stageState == BLANK) {
							say("====================================The first time to stage chunks %d.\n", i);
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
				// all the chunks of current SID are staged, go on to handle next SID
				if (s == -1) {
					continue;
				}
				int e = s + SIDToWindow[SID];
				if (e > (int)(*I).second.size()) {
					e = (int)(*I).second.size();
				}
				say("s = %d, e = %d.\n", s, e);
//cerr<<s<<"\t"<<e<<endl;
				pthread_mutex_lock(&profileLock);
				// only update the state of the chunks not staged yet
				for (int i = s; i < e; i++) {
					if (SIDToProfile[SID][(*I).second[i]].stageState == BLANK) {
//cerr<<(*I).second[i]<<endl;
						CIDs.push_back((*I).second[i]);
						SIDToProfile[SID][(*I).second[i]].stageState = PENDING;
						SIDToProfile[SID][(*I).second[i]].fetchAD = stageAD;
						SIDToProfile[SID][(*I).second[i]].fetchHID = stageHID;
					}
					say("fetchAD of CID: %s is: %s\n", string2char((*I).second[i]), string2char(SIDToProfile[SID][(*I).second[i]].fetchAD));
				}
				pthread_mutex_unlock(&profileLock);

				// send the staging list to the stage service and receive message back and update stage state
				// TODO: non-block fasion
				say("The size of CID is %d.\n", CIDs.size());
				if (CIDs.size() > 0) {
					sprintf(cmd, "stage");
					for (unsigned i = 0; i < CIDs.size(); i++) {
						strcat(cmd, " ");
						strcat(cmd, string2char(CIDs[i]));
					}
					//cerr << "The current command is: " << cmd << "\n";
					say("Before manager send command to server: %s.\n", cmd);
					sendStreamCmd(netStageSock, cmd);
					say("After manager send command to server: %s.\n", cmd);

					char send_to_stage_server_[XIA_MAX_BUF];
					memset(send_to_stage_server_, '\0', strlen(send_to_stage_server_));
					sprintf(send_to_stage_server_, "Ready to receive chunk ready message from stage server.");
					// receive chunk ready msg one by one
					for (unsigned i = 0; i < CIDs.size(); i++) {
						memset(reply, '\0', strlen(reply));
						sayHello(netStageSock, send_to_stage_server_);
						if ((n = Xrecv(netStageSock, reply, sizeof(reply), 0)) < 0) {
							Xclose(netStageSock);
							die(-1, "Unable to communicate with the server\n");
						}
//cerr<<reply<<endl;
						// update "ready" to stage state
						if (strncmp(reply, "ready", 5) == 0) {
							vector<string> staged_info = strVector(reply + 6);
							if (CIDs[i] == staged_info[0]) {
								pthread_mutex_lock(&profileLock);
								SIDToProfile[SID][CIDs[i]].stageStartTimestamp = string2long(staged_info[1]);
								SIDToProfile[SID][CIDs[i]].stageFinishTimestamp = string2long(staged_info[2]);
								SIDToProfile[SID][CIDs[i]].stageState = READY;
								say("StageData----------------CID: %s is ready.\n", string2char(staged_info[0]));
								//pthread_mutex_unlock(&StageFetchLock);
								pthread_mutex_unlock(&profileLock);
							}
						}
					}
/*
					// receive chunk ready msg in batch
					if ((n = Xrecv(netStageSock, reply, sizeof(reply), 0)) < 0) {
						Xclose(netStageSock);
						die(-1, "Unable to communicate with the server\n");
					}
	//cerr<<reply<<endl;
					// update "ready" to stage state
					if (strncmp(reply, "ready", 5) == 0) {
						vector<string> CIDs_staging = strVector(reply+6);
						pthread_mutex_lock(&profileLock);
						for (unsigned i = 0; i < CIDs_staging.size(); i++) {
							SIDToProfile[SID][CIDs_staging[i]].stageState = READY;
						}
						pthread_mutex_unlock(&profileLock);
					}
*/
                }
			}
		}
		// after looping all the SIDs
		if (finish) {
			say("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!Kill stageData thread.\n");
			pthread_exit(NULL);
		}
		else
			pthread_mutex_unlock(&FetchControl);
		usleep(SCAN_DELAY_MSEC*1000);
		//pthread_mutex_unlock(&fetchLock);
	}
	pthread_exit(NULL);
}

// WORKING version
// data plane of staging: find the pending chunk, construct the dag and send the pending chunk request,
//void *fetchData(void *socketid)
void *fetchData(void *)
{
	pthread_mutex_lock(&FetchControl);
	int chunkSock;
    //int clientSock = *((int*)socketid);
	char fetchinfo_send_to_client_[XIA_MAX_BUF];
	memset(fetchinfo_send_to_client_, '\0', strlen(fetchinfo_send_to_client_));
	sprintf(fetchinfo_send_to_client_, "Finish fetch current CID.");

	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");

	int status = -1;

	while (1) {
		pthread_mutex_lock(&FetchControl);
		say("First===========================================In while loop of fetchData.\n");
		//pthread_mutex_lock(&fetchLock);
		//pthread_cond_wait(&fetchCond, &fetchLock);
		//pthread_mutex_lock(&StageFetchLock);
		for (map<string, vector<string> >::iterator I = SIDToCIDs.begin(); I != SIDToCIDs.end(); ++I) {
			if ((*I).second.size() > 0) {// && SIDToProfile[(*I).first][(*I).second[(*I).second.size()-1]].fetchState != READY) {
				string SID = (*I).first;
//cerr<<"SID: "<<SID<<"chunks to be pulled from the queue\n";
				vector<string> CIDs_fetching;
				// find the CID to fetch behalf of the xftp client
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
					if (SIDToProfile[SID][(*I).second[i]].fetchState == PENDING && SIDToProfile[SID][(*I).second[i]].stageState == READY) {
cerr<<(*I).second[i]<<"\t fetch state: PENDING; \t stage state: READY\n";
						CIDs_fetching.push_back((*I).second[i]);
					}
				}
				say("FetchData-----------------------------Size of CIDs is %d\n", CIDs_fetching.size());
				if (CIDs_fetching.size() > 0) {
					// TODO: check the size is smaller than the max chunk to fetch at one time
					ChunkStatus cs[CIDs_fetching.size()];
					int n = CIDs_fetching.size();

					unsigned ctr = 0;

					for (unsigned int i = 0; i < CIDs_fetching.size(); i++) {
						pthread_mutex_lock(&profileLock);
						SIDToProfile[SID][CIDs_fetching[i]].fetchStartTimestamp = now_msec();
						pthread_mutex_unlock(&profileLock);
					}

					while (1) {
						say("Second===========================================In while loop of fetchData.\n");
						// Retransmit chunks request every REREQUEST seconds if not ready
						for (unsigned int i = 0; i < CIDs_fetching.size(); i++) {
							char *dag = (char *)malloc(512);
							sprintf(dag, "RE ( %s %s ) CID:%s", string2char(SIDToProfile[SID][CIDs_fetching[i]].fetchAD), string2char(SIDToProfile[SID][CIDs_fetching[i]].fetchHID), string2char(CIDs_fetching[i]));
							cerr << dag << endl;
							cs[i].cidLen = strlen(dag);
							cs[i].cid = dag; // cs[i].cid is a DAG, not just the CID
							//free(dag); chenren: why freeze the program!!!!! see error msg below
							// [libprotobuf ERROR google/protobuf/wire_format.cc:1053] String field contains invalid UTF-8 data when parsing a protocol buffer. Use the 'bytes' type if you intend to send raw bytes.
							//click: ../include/click/vector.hh:59: const T& Vector<T>::operator[](Vector<T>::size_type) const [with T = XIAPath::Node; Vector<T>::size_type = int]: Assertion `(unsigned) i < (unsigned) _n' failed.
						}

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
//cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^manager: chunksock is " << chunkSock << endl;
//cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^manager: cs.cid is" << cs[0].cid << endl;
//cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^manager: cs.cidLen is" << cs[0].cidLen << endl;
//cerr << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^manager: n is" << n << endl;
						status = XgetChunkStatuses(chunkSock, cs, n);

						if (status == READY_TO_READ) {
							pthread_mutex_lock(&profileLock);
							for (unsigned int i = 0; i < CIDs_fetching.size(); i++) {
								SIDToProfile[SID][CIDs_fetching[i]].fetchState = READY;
cerr<<CIDs_fetching[i]<<"\t fetch state: ready\n";
//say("fetchData````````````````````````````The clientSock is %d\n", clientSock);
//sayHello(clientSock, fetchinfo_send_to_client_);
                                //pthread_mutex_unlock(&FetchDelegationLock);
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

						usleep(CHUNK_REQUEST_DELAY_MSEC*1000);
					}
				}
				// TODO: timeout the chunks by free(cs[i].cid); cs[j].cid = NULL; cs[j].cidLen = 0;
			}
		}
		usleep(LOOP_DELAY_MSEC*1000);
		//pthread_mutex_unlock(&fetchLock);
	}
	//Xclose(clientSock);
	// 	big loop, SIDToProfile[remoteSID][CID].fetchState = PENDING and stage state == done, construct dag, fetch data, update
	pthread_exit(NULL);
}

// TODO: make it as a library
void *netMon(void *)
{
	ofstream mfile;
	mfile.open("ssid.txt", ios::app);

	string lastTempSSID = execSystem(GETSSID_CMD);
	string currTempSSID;
	mfile<<now_msec()<<"\t"<<lastTempSSID<<endl;

	while (1) {
		currTempSSID = execSystem(GETSSID_CMD);
		if (lastTempSSID != currTempSSID) {
			lastTempSSID = currTempSSID;
			if (currTempSSID == "") {
				mfile<<now_msec()<<"\tNA"<<endl;
			}
			else {
				mfile<<now_msec()<<"\t"<<currTempSSID<<endl;
			}
		}
		usleep(SCAN_DELAY_MSEC*1000);
	}

/*
	int sock;

	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		die(-1, "Unable to create the listening socket\n");
	}

	netChange = false;
	netChangeACK = false;
	char last_ad[MAX_XID_SIZE], curr_ad[MAX_XID_SIZE], hid[MAX_XID_SIZE], ip[MAX_XID_SIZE];

	if (XreadLocalHostAddr(sock, curr_ad, sizeof(curr_ad), hid, sizeof(hid), ip, sizeof(ip)) < 0) {
		die(-1, "Reading localhost address\n");
	}

	strcpy(last_ad, curr_ad);

	while (1)
	{
		if (XreadLocalHostAddr(sock, curr_ad, sizeof(curr_ad), hid, sizeof(hid), ip, sizeof(ip)) < 0) {
			die(-1, "Reading localhost address\n");
		}
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
		// TODO: to replace with the following with a special socket added later
		//struct pollfd pfds[2];
		//pfds[0].fd = sock;
		//pfds[0].events = POLLIN;
		//if ((rc = Xpoll(pfds, 1, 5000)) <= 0) {
		//	die(-5, "Poll returned %d\n", rc);
		//}
		usleep(LOOP_DELAY_MSEC*1000);
	}
*/
	pthread_exit(NULL);
}

void *winPred(void *)
{
	long fetch_latency;
	long stage_latency;
	while (1) {
		say("In winPred.\n");
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

// TODO: if the SID's all CIDs have READY fetch state, then remove
void *profileMgt(void *)
{
	while (1) {
		say("In profileMgt.\n");
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
	ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID); // get ftpServAD and ftpServHID for building chunk request
	lastSSID = getSSID();
	currSSID = lastSSID;
	strcpy(currAD, myAD);
	stageSock = registerStreamReceiver(getStageManagerName(), myAD, myHID, my4ID); // communicate with client app

	//pthread_t thread_winPred, thread_mgt, thread_netMon, thread_fetchData, thread_stageData, thread_client; // TODO: thread_netMon
	pthread_t thread_winPred, thread_mgt, thread_netMon, thread_stageData,thread_fetchData; // TODO: thread_netMon
	pthread_create(&thread_netMon, NULL, netMon, NULL);	// TODO: improve the netMon function
	//pthread_create(&thread_winPred, NULL, winPred, NULL);	// TODO: improve the netMon function
	pthread_create(&thread_mgt, NULL, profileMgt, NULL); // dequeue, stage and update profile
	//pthread_create(&thread_fetchData, NULL, fetchData, (void *)&acceptSock);
	pthread_create(&thread_fetchData, NULL, fetchData, NULL);

    pthread_create(&thread_stageData, NULL, stageData, NULL);

	//say("The current stageSock is %d\n", stageSock);
	//blockListener((void *)&stageSock, clientCmd);
	//twoFunctionBlockListener((void *)&stageSock, clientCmd, fetchData);
    blockListener((void*)&stageSock,clientCmd);
	return 0;
}
