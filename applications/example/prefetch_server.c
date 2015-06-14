#include "prefetch_utils.h"

using namespace std;

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char remoteAD[MAX_XID_SIZE];
char remoteHID[MAX_XID_SIZE];
char remoteSID[MAX_XID_SIZE];

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

// TODO: optimize sleep(1)
// TODO: trafficMon function to update prefetching window
// TODO: move retransmit request from app to xtransport?

int profileServerSock, ftpSock;

pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t windowLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bufLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t timeLock = PTHREAD_MUTEX_INITIALIZER;

#define BLANK 0	// initilized
#define REQ 1 	// requested by router
#define DONE 2	// available in cache

#define PURGE_SEC 120

struct chunkStatus {
	string CID;
	long timestamp; // msec
	bool fetch;
	int prefetch;
};

map<string, vector<chunkStatus> > profile;
map<string, unsigned int> window; // prefetching window
map<string, vector<string> > buf; // chunk buffer to prefetch
map<string, string> SIDToAD; // store the mapping between SID and AD of peer. Once the SID move to another AD, stop prefetching
map<string, long> SIDToTime; // store the timestamp last seen

int prefetch_chunk_num = 3; // default number of chunks to prefetch

void reg_handler(int sock, char *cmd) 
{
	// cmd format: reg cid_num cid1 cid2 ...
	char cmd_arr[strlen(cmd)];
	strcpy(cmd_arr, cmd);

	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); // get client ftp's

	if (strcmp(remoteAD, myAD) != 0) {
cerr<<"Reject reg from non-local request"<<endl;
		return;
	}

	pthread_mutex_lock(&windowLock);	
	window[remoteSID] = prefetch_chunk_num; // TODO: to optimize
	pthread_mutex_unlock(&windowLock);

	vector<string> CIDs;
	pthread_mutex_lock(&windowLock);	
	buf[remoteSID] = CIDs;
	pthread_mutex_unlock(&windowLock);

cerr<<cmd_arr<<endl;

	strtok(cmd_arr, " "); // skip the "reg"

	vector<chunkStatus> css;

	char *CID_temp;
	while ((CID_temp = strtok(NULL, " ")) != NULL) {
		chunkStatus cs;
		cs.CID = CID_temp;
		cs.timestamp = now_msec(); 
		cs.fetch = false;
		cs.prefetch = BLANK;
		css.push_back(cs);
	} 

	pthread_mutex_lock(&profileLock);	
	if (profile.count(remoteSID)) { 
		profile.erase(remoteSID);
cerr<<"SID is registered before, being replaced...\n";
	}
	profile[remoteSID] = css;

	pthread_mutex_lock(&bufLock);
	for (unsigned int i = 0; i < window[remoteSID]; i++) {
		// TODO: looks ugly, improve later: if i < profile[SID].size(), blablabla
		if (i == profile[remoteSID].size()) {
			break;
		}
		if (profile[remoteSID][i].prefetch == BLANK) { 
			buf[remoteSID].push_back(profile[remoteSID][i].CID);
			profile[remoteSID][i].prefetch = REQ; // marked as requested by router
		}
	}
	pthread_mutex_unlock(&bufLock);
	pthread_mutex_unlock(&profileLock);

cerr<<"\nPrint profile table in reg_handler\n";
	for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
		for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
		}
	}	

	// set the mapping up to be checked later for polling
	SIDToAD[remoteSID] = remoteAD;

	pthread_mutex_lock(&timeLock);
	SIDToTime[remoteSID] = now_msec();
	pthread_mutex_unlock(&timeLock);
cerr<<"Finish reg"<<endl;

	return;
}

void poll_handler(int sock, char *cmd) 
{ 
	char cmd_arr[strlen(cmd)];
	strcpy(cmd_arr, cmd);
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); // get client ftp's SID
cerr<<"Peer SID: "<<remoteSID<<endl; 
	strtok(cmd_arr, " "); // skip the "fetch"
	string CID = strtok(NULL, " ");
cerr<<CID<<endl;

	// reply format: available cid1 cid2, ...	
	char reply[XIA_MAX_BUF] = "available";
cerr<<"Reply msg is initialized: "<<reply<<endl;
	pthread_mutex_lock(&profileLock);
	pthread_mutex_lock(&bufLock);

	unsigned int p = 0;
	// find the requested CID index  
	while (p < profile[remoteSID].size()) {
		if (profile[remoteSID][p].CID == CID) 
			break;
		p++;
	}

	cerr<<"Chunks are avaiable starting from index "<<p<<endl;
	unsigned int c = 0;
	unsigned i;
	// return all the CIDs available starting from p
	for (i = p; i < profile[remoteSID].size(); i++) {
		// mark fetch as "true" when the chunk is available after probe				
		if (profile[remoteSID][i].prefetch == DONE) {
			profile[remoteSID][i].fetch = true;
			strcat(reply, " ");
			strcat(reply, string2char(profile[remoteSID][i].CID));
			c++;
		}
	}

	if (c == 0) 
		strcat(reply, " none");	

cerr<<"Reply: "<<reply<<endl;

	if (remoteAD == SIDToAD[remoteSID]) {
cerr<<"Same network, continute to prefetch\n";
		// find the first chunk not fetched yet
		p = 0;
		while (p < profile[remoteSID].size()) {
			if (profile[remoteSID][p].fetch == false) 
				break;
			p++;
		}
cerr<<"Prefetching chunks starting from index: "<<p<<endl;
		// push push the chunks and marked as requested range from p to p + window
		i = p;
		while (i < p + window[remoteSID]) {
			// TODO: looks ugly, improve later: if i < profile[SID].size(), blablabla
			if (i == profile[remoteSID].size()) 
				break;		
			if (profile[remoteSID][i].prefetch == BLANK) { 
				buf[remoteSID].push_back(profile[remoteSID][i].CID);
				profile[remoteSID][i].prefetch = REQ; // marked as requested by router
			}
			i++;
			if (i == profile[remoteSID].size()) 
				break;
		}

		say("\nPrint profile table in poll_handler\n");
		for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
			for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
			}
		}	
cerr<<"SID: "<<remoteSID<<" - chunks to be pushed into the queue:\n";
		for (unsigned i = 0; i < buf[remoteSID].size(); i++)
cerr<<buf[remoteSID][i]<<endl;
	}
	else {
cerr<<"Client network changed, stop prefetching\n";
	}

	pthread_mutex_unlock(&bufLock);	
	pthread_mutex_unlock(&profileLock);

	pthread_mutex_lock(&timeLock);	
	SIDToTime[remoteSID] = now_msec();
	pthread_mutex_unlock(&timeLock);

	sendStreamCmd(sock, reply);

	return;
}

void *clientReqCmd(void *socketid) 
{
 	// stream protocol
	char cmd[XIA_MAXBUF];
	int sock = *((int*)socketid);
	int n = -1;

	while (1) {
		memset(cmd, '\0', XIA_MAXBUF);
		if ((n = Xrecv(sock, cmd, sizeof(cmd), 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		// reg msg from xftp client
		if (strncmp(cmd, "reg", 3) == 0) {
			say("Receive a reg message\n");
			reg_handler(sock, cmd);
		}
		// fetch probe from xftp client: fetch CID
		else if (strncmp(cmd, "poll", 4) == 0) {
			say("Receive a polling message\n");
			poll_handler(sock, cmd);
		}	
	}

	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

// TODO: pktMon
void *prefetchPred(void *) 
{
	// go through all the SID
	while (1) {
		//prefetch_chunk_num = 3;		
	}
	pthread_exit(NULL);
}

void *prefetchExec(void *) 
{
	while (1) {
		// update DONE to profile
		pthread_mutex_lock(&profileLock);
		// pop out the chunks for prefetching 
		pthread_mutex_lock(&bufLock);	
		for (map<string, vector<string> >::iterator I = buf.begin(); I != buf.end(); ++I) {
			if ((*I).second.size() > 0) {
				string SID = (*I).first;
cerr<<"SID: "<<SID<<"chunks to be pulled from the queue\n";

				for (unsigned int i = 0; i < (*I).second.size(); i++) {
cerr<<(*I).second[i]<<endl;
				}
				int chunkSock;

				if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
					die(-1, "unable to create chunk socket\n");

				ChunkStatus cs[(*I).second.size()];
				char data[XIA_MAXCHUNK];
				int len;
				int status;
				int n = (*I).second.size();

				for (unsigned int i = 0; i < (*I).second.size(); i++) {
					char *dag = (char *)malloc(512);
					sprintf(dag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, string2char((*I).second[i]));
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

				for (unsigned int i = 0; i < (*I).second.size(); i++) {
					if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
						say("error getting chunk\n");
						pthread_exit(NULL); // TODO: check again
					}
					else {
cerr<<"update chunks DONE information...\n";
						for (unsigned int j = 0; j < profile[SID].size(); j++) {
							if (profile[SID][j].CID == (*I).second[i]) 
								profile[SID][j].prefetch = DONE;
						}							
					}
				}
				(*I).second.clear();
				// TODO: timeout the chunks by free(cs[i].cid); cs[j].cid = NULL; cs[j].cidLen = 0;			
			}
		} 
//cerr<<"\nPrint profile table after updating DONE\n";
		for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
			for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
			}
		}	

		pthread_mutex_unlock(&bufLock);
	 	pthread_mutex_unlock(&profileLock);

		sleep(1);
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
				profile.erase(SID);
				pthread_mutex_unlock(&profileLock);
				pthread_mutex_lock(&windowLock);
				window.erase(SID);
				pthread_mutex_unlock(&windowLock);
				pthread_mutex_lock(&bufLock);
				buf.erase(SID);
				pthread_mutex_unlock(&bufLock);
			}
		}	
		sleep(3);
	}
	pthread_exit(NULL);	
}

int main() 
{
	profileServerSock = registerStreamReceiver(getPrefetchServiceName(), myAD, myHID, my4ID);

	ftpSock = initStreamClient(FTP_NAME, myAD, myHID, ftpServAD, ftpServHID); // get ftpServAD and ftpServHID for building chunk request

	pthread_t thread_pred, thread_exec, thread_mgt;
	pthread_create(&thread_pred, NULL, prefetchPred, NULL);	// TODO: 
	pthread_create(&thread_exec, NULL, prefetchExec, NULL); // dequeue, prefetch and update profile
	pthread_create(&thread_mgt, NULL, profileMgt, NULL); // dequeue, prefetch and update profile

	blockListener((void *)&profileServerSock, clientReqCmd);

	return 0;	
}
