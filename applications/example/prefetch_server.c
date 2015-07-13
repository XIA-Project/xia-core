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

int prefetchServerSock, ftpSock;

pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bufLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t timeLock = PTHREAD_MUTEX_INITIALIZER;

map<string, map<string, bool> > SIDToProfile;	// prefetch state
map<string, vector<string> > SIDToBuf; // chunk buffer to prefetch
map<string, long> SIDToTime; // timestamp last seen

// TODO: make it possible that prefetch client can send another prefetching request before get a ready response
void prefetchControl(int sock, char *cmd) 
{ 
	XgetRemoteAddr(sock, remoteAD, remoteHID, remoteSID); // get prefetch client's SID
//cerr<<"Peer SID: "<<remoteSID<<endl; 

	vector<string> CIDs = cidList(cmd);

	pthread_mutex_lock(&profileLock);	
	for (unsigned int i = 0; i < CIDs.size(); i++) {
		SIDToProfile[remoteSID][CIDs[i]] = false;
	}
	pthread_mutex_unlock(&profileLock);	

	// put the CIDs into the buffer to be prefetched
	pthread_mutex_lock(&bufLock);	
	if (SIDToBuf.count(remoteSID)) { 
		for (unsigned int i = 0; i < CIDs.size(); i++) {
			SIDToBuf[remoteSID].push_back(CIDs[i]);
		}
	}
	else {
		SIDToBuf[remoteSID] = CIDs;		
	}
	pthread_mutex_unlock(&bufLock);		

	pthread_mutex_lock(&timeLock);	
	SIDToTime[remoteSID] = now_msec();
	pthread_mutex_unlock(&timeLock);

	// TODO: send the chunk ready msg one by one; now waiting until all the chunks are ready
	// wait until all the chunks are ready and send it back
	while (1) {
		unsigned int c = 0;
		for (unsigned int i = 0; i < CIDs.size(); i++) {
			if (SIDToProfile[remoteSID][CIDs[i]] == true) {
				c++;	
			}
		}
		if (c == CIDs.size()) {
			char reply[XIA_MAX_BUF] = "ready";
			for (unsigned int i = 0; i < CIDs.size(); i++) {
				strcat(reply, " ");
				strcat(reply, string2char(CIDs[i]));		
			}
			sendStreamCmd(sock, reply); 	
			return;	
		}
		usleep(LOOP_DELAY_MSEC*1000);
	}
	return;
}

void *prefetchCmd(void *socketid) 
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
		if (strncmp(cmd, "prefetch", 8) == 0) {
			say("Receive a prefetch message\n");
			prefetchControl(sock, cmd+9);
		}	
	}

	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

// read the CID from the prefetch buffer, execute prefetching, and update profile 
// TODO: paralize getting chunks for each SID, i.e. fair scheduling
void *prefetchData(void *) 
{
	while (1) {
		// pop out the chunks for prefetching 
		pthread_mutex_lock(&bufLock);	
		for (map<string, vector<string> >::iterator I = SIDToBuf.begin(); I != SIDToBuf.end(); ++I) {
			if ((*I).second.size() > 0) {
				string SID = (*I).first;
//cerr<<"SID: "<<SID<<endl;
//cerr<<"Chunks to be pulled from the queue:\n";
				for (unsigned int i = 0; i < (*I).second.size(); i++) {
//cerr<<(*I).second[i]<<endl;
				}
				int chunkSock;

				if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
					die(-1, "unable to create chunk socket\n");

				// TODO: check the size is smaller than the max chunk to fetch at one time
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

					usleep(CHUNK_REQUEST_DELAY_MSEC*1000); 
				}

				for (unsigned int i = 0; i < (*I).second.size(); i++) {
					if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
						say("error getting chunk\n");
						pthread_exit(NULL); // TODO: check again
					}
					else {
						pthread_mutex_lock(&profileLock);
						SIDToProfile[SID][(*I).second[i]] = true;
	 					pthread_mutex_unlock(&profileLock);
					}
				}
				(*I).second.clear();
				// TODO: timeout the chunks by free(cs[i].cid); cs[j].cid = NULL; cs[j].cidLen = 0;			
			}
		} 
		pthread_mutex_unlock(&bufLock);

		sleep(1);
	}
	pthread_exit(NULL);
}

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
				pthread_mutex_unlock(&profileLock);
				pthread_mutex_lock(&bufLock);
				SIDToBuf.erase(SID);
				pthread_mutex_unlock(&bufLock);
			}
		}	
		sleep(MGT_DELAY_SEC);
	}
	pthread_exit(NULL);	
}

int main() 
{
	pthread_t thread_prefetch, thread_mgt;
	pthread_create(&thread_prefetch, NULL, prefetchData, NULL); // dequeue, prefetch and update profile
	pthread_create(&thread_mgt, NULL, profileMgt, NULL); // dequeue, prefetch and update profile

	ftpSock = initStreamClient(getXftpName(), myAD, myHID, ftpServAD, ftpServHID); // get ftpServAD and ftpServHID for building chunk request
	prefetchServerSock = registerStreamReceiver(getPrefetchServiceName(), myAD, myHID, my4ID);
	blockListener((void *)&prefetchServerSock, prefetchCmd);

	return 0;	
}

/*	cerr<<"\nPrint profile table after updating DONE\n";
		for (map<string, vector<chunkStatus> >::iterator I = SIDToProfile.begin(); I != SIDToProfile.end(); ++I) {
			for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
cerr<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).prefetch<<endl;
			}
	}	
*/
