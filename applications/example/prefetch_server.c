#include "prefetch_utils.h"

using namespace std;

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

char prefetchPredAD[MAX_XID_SIZE];
char prefetchPredHID[MAX_XID_SIZE];

char prefetchExecAD[MAX_XID_SIZE];
char prefetchExecHID[MAX_XID_SIZE];

char ftpServAD[MAX_XID_SIZE];
char ftpServHID[MAX_XID_SIZE];

char prefetch_profile_name[] = "www_s.profile.prefetch.aaa.xia";
//char prefetch_pred_name[] = "www_s.prediction.prefetch.aaa.xia";
//char prefetch_exec_name[] = "www_s.executer.prefetch.aaa.xia";
char ftp_name[] = "www_s.ftp.advanced.aaa.xia";


// TODO: profile figure out the chunks and put into queues
// TODO: optimize sleep(1)
// TODO: netMon function to update prefetching window
// Question: how many chunks can API support to get in one time? can we stopping retransmit request in application

// profile section starts
//int profileServerSock, profilePredSock;
int profileServerSock, ftpSock;

pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t windowLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bufLock = PTHREAD_MUTEX_INITIALIZER;

#define BLANK 0; // initilized
#define REQ 1; // requested by router
#define DONE 2; // available in cache

struct chunkStatus {
	char *CID;
	long timestamp; // msec
	bool fetch;
	int prefetch;
};

map<char *, vector<chunkStatus> > profile;
map<char *, int> window; // prefetching window
map<char *, vector<char *> > buf; // chunk buffer to prefetch

int prefetch_chunk_num = 3; // default number of chunks to prefetch

void reg_handler(int sock, char *cmd) {
	// format: reg cid_num cid1 cid2 ...
	char cmd_arr[strlen(cmd)];
	strcpy(cmd_arr, cmd);
	char *SID = XgetRemoteSID(sock); // get client ftp's SID
	strtok(cmd_arr, " "); // skip the "reg"
	int cid_num = atoi(strtok(NULL, " "));
	vector<chunkStatus> css;
	for (int i = 0; i < cid_num; i++) {
		chunkStatus cs;
		cs.CID = strtok(NULL, " ");
		cs.timestamp = now_msec(); 
		cs.fetch = false;
		cs.prefetch = BLANK;
		css.push_back(cs);
	}
	pthread_mutex_lock(&profileLock);	
	profile[SID] = css;
	pthread_mutex_unlock(&profileLock);
	pthread_mutex_lock(&windowLock);	
	window[SID] = prefetch_chunk_num;
	pthread_mutex_unlock(&windowLock);	
}

void poll_handler(int sock, char *cmd) {

	char cmd_arr[strlen(cmd)];
	strcpy(cmd_arr, cmd);
	char *SID = XgetRemoteSID(sock);
	strtok(cmd_arr, " "); // skip the "fetch"
	char *CID = strtok(NULL, " ");

	char reply[XIA_MAX_BUF];
	// reply format: available cid1 cid2, ...
	//strcat(reply, "available");
	pthread_mutex_lock(&profileLock);
	vector<chunkStatus> css = profile[SID];

	// return the CIDs which chunks are available 
	for (unsigned int i = 0; i < css.size(); i++) {
		if (strcmp(css[i].CID, CID) == 0) {
			unsigned j = i;
			while (1) {
				// mark fetch as "true" when the chunk is available after probe				
				if (css[j].prefetch == 2) {
					css[j].fetch = true;
					strcat(reply, " ");										
					strcat(reply, css[i].CID);
				}
				// if the current CID is neither initlized nor requested by router, then report none and wait
				else {
					if (j == i) strcat(reply, " none");	
					break;
				}
				j++;
			}
		}
	}

	pthread_mutex_lock(&bufLock);

	vector<char *> CIDs = buf[SID];

	for (unsigned int i = 0; i < css.size(); i++) {
		// find the first one not fetched by client yet		
		if (css[i].fetch == false) {
			for (unsigned int j = i; j < i + window[SID]; j++) {
				if (css[j].prefetch == 0) { 
					CIDs.push_back(css[j].CID);
					css[j].prefetch = REQ; // marked as requested by router
				}
			}
		}
	}
	pthread_mutex_unlock(&bufLock);	

	pthread_mutex_unlock(&profileLock);
	sendCmd(sock, reply);
}

// TODO
void *netMon(void *) {
	// go through all the SID
	while (1) {
		prefetch_chunk_num = 3;		
	}
	pthread_exit(NULL);
}

void *prefetchExec(void *) {

	while (1) {
		// pop out the chunks for prefetching 
		pthread_mutex_lock(&bufLock);	

		for (map<char *, vector<char *> >::iterator I = buf.begin(); I != buf.end(); ++I) {
			if ((*I).second.size() > 0) {
				char *SID = (*I).first;
				vector<char *> CIDs = (*I).second; // or I->second // TODO: check!

				int chunkSock;

				if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
					die(-1, "unable to create chunk socket\n");

				ChunkStatus cs[CIDs.size()];
				char data[XIA_MAXCHUNK];
				int len;
				int status;
				int n = CIDs.size();

				for (unsigned int i = 0; i < CIDs.size(); i++) {
					char *dag = (char *)malloc(512);
					sprintf(dag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, CIDs[i]);
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

				// update DONE to profile
	  		pthread_mutex_lock(&profileLock);

				vector<chunkStatus> css = profile[SID];

				for (unsigned int i = 0; i < CIDs.size(); i++) {
					if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
						say("error getting chunk\n");
						pthread_exit(NULL); // TODO: check again
					}
					else {
						for (unsigned int j = 0; j < css.size(); j++) {
							if (strcmp(css[j].CID, CIDs[i]) == 0) 
								css[j].prefetch = DONE;
						}							
					}
				}

	 	 		pthread_mutex_unlock(&profileLock);

				(*I).second.clear();
				// TODO: timeout the chunks by free(cs[i].cid); cs[j].cid = NULL; cs[j].cidLen = 0;			
				//for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
				//	cout<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
				//}
			}
		}
		pthread_mutex_unlock(&bufLock);
		sleep(1);
	}
	pthread_exit(NULL);
}

/*
void prefetch_pred_handler(int sock, char *cmd) {
	// format: prefetch SID prefetch_chunk_num 
	char cmd_arr[strlen(cmd)];
	strcpy(cmd_arr, cmd);
	strtok(cmd_arr, " "); // skip the "prefetch"
	char *SID = strtok(NULL, " ");
	int prefetch_chunk_num = atoi(strtok(NULL, " "));

	// format: prefetch cid1 cid2 ... 
	char reply[XIA_MAX_BUF];
	strcat(reply, "prefetch");

	pthread_mutex_lock(&profileLock);
	vector<chunkStatus> css = profile[SID];
	int c = 0;
	for (unsigned int i = 0; i < css.size(); i++) {
		// find the first one not fetched by client yet		
		if (css[i].fetch == false) {
			for (unsigned int j = i; j < i + prefetch_chunk_num; j++) {
				if (css[j].prefetch == 0) { 
					strcat(reply, " ");						
					strcat(reply, css[j].CID);
					css[j].prefetch = 1; // marked as requested by router
					c++;
				}
			}
		}
	}
	if (c == 0)
		strcat(reply, " none");

	pthread_mutex_unlock(&profileLock);	
	sendCmd(sock, reply);	
}

void prefetch_exec_handler(int sock, char *cmd) {
	// format: prefetched cid1 cid2 ...
	char cmd_arr[strlen(cmd)];
	strcpy(cmd_arr, cmd);
	char *SID = XgetRemoteSID(sock); // get client ftp's SID
	strtok(cmd_arr, " "); // skip the "prefetched"
	//int cid_num = atoi(strtok(NULL, " "));

	pthread_mutex_lock(&profileLock);
	vector<chunkStatus> css = profile[SID];
	char* CID;
	while ((CID = strtok(NULL, " ")) != NULL) {
		for (unsigned int i = 0; i < css.size(); i++) {
			if (strcmp(css[i].CID, CID) == 0) 
				css[i].prefetch = DONE;
		}		
	}

  pthread_mutex_unlock(&profileLock);
}
*/
void *clientReqCmd (void *socketid) {

	char cmd[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	int n;

	while (1) {
		memset(cmd, '\0', strlen(cmd));
		memset(reply, '\0', strlen(reply));
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
			say("Receive a polling message");
			poll_handler(sock, cmd);
		}
		/*
		// prefetch request from prefetch pred
		else if (strncmp(cmd, "prefetch", 8) == 0) {
			prefetch_pred_handler(sock, cmd);
		}
		// chunk available update from prefetch exec
		else if (strncmp(cmd, "prefetched", 10) == 0) {
			prefetch_exec_handler(sock, cmd);
		}
		*/
		
		for (map<char *, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
			for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
				cout<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).fetch<<"\t"<<(*J).prefetch<<endl;
			}
		}	
		
	}
	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}
// profile section ends

/*
// prediction section starts
int	predProfileSock, predServerSock, predExecSock;

vector<char *> SIDs;

int prefetch_chunk_num = 3; // number of chunks to prefetch

// TODO: incoporate BW estimation to figure out number of chunks to prefetch
// nitin: how to manage different connections (client SID)? each one should be a thread? 
// which variables belong to all threads and which for local thread?

// receive the SID from prefetch profile 
void *regCmd (void *socketid) {

	char cmd[XIA_MAXBUF]; // receive from prefetch profile of the new SID 
	int sock = *((int*)socketid);
	int n = -1;

	while (1) {

		memset(cmd, '\0', strlen(cmd));

		// receive SID from prefetch profile after xftp client register with it
		if ((n = Xrecv(sock, cmd, sizeof(cmd), 0)) < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}

		if (strncmp(cmd, "sid", 3) == 0) {
			SIDs.push_back(strtok(cmd, " ")); // TODO: change it is not existed before
		}
	}
	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

// send number of chunks to prefetch and get CIDs from prefetch_profile, and send the list to prefetch_exec
int prefetch(int predProfileSock, int predExecSock) {

	char cmd[XIA_MAXBUF];
	char reply[XIA_MAXBUF];

	int n = -1;

	// get chunks to prefetch for different SIDs one by one, and forward to prefetch executer
	while (1) {
		// TODO: optimize: put different SID's into one msg
		for (unsigned int i = 0; i < SIDs.size(); i++) {
			memset(cmd, '\0', strlen(cmd));
			memset(reply, '\0', strlen(reply));
			sprintf(cmd, "prefetch %s %d", SIDs[i], prefetch_chunk_num);
			sendCmd(predProfileSock, cmd);
			if ((n = Xrecv(predProfileSock, reply, sizeof(reply), 0))  < 0) {
				warn("socket error while waiting for data, closing connection\n");
				break;
			}
			// forward the CID to prefetch to prefetch executer if any
			if (strncmp(reply, "prefetch", 8) == 0 && strncmp(reply, "prefetch none", 13) != 0) {
				sendCmd(predExecSock, reply);
			}
		}
		usleep(100000); // TODO: investigate 
	}
	return -1;
}
// prediction section ends


// execution section starts
int	ftpSock, execProfileSock, execServerSock;

void *execCmd (void *socketid) {

	char cmd[XIA_MAXBUF]; // receive from 
	char reply[XIA_MAXBUF]; // update the profile
	int sock = *((int*)socketid);
	int n = -1;

	while (1) {

		memset(cmd, '\0', strlen(cmd));
		memset(reply, '\0', strlen(reply));

		if ((n = Xrecv(sock, cmd, sizeof(cmd), 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}

		if (strncmp(cmd, "prefetch", 8) == 0) {
			char cmd_arr[strlen(cmd)];
			strcpy(cmd_arr, cmd);
			strtok(cmd_arr, " ");
			// vector<char *> CIDs;

			strcat(reply, "prefetched");
			// char *cid_num_str; 			
			// sprintf(cid_num_str, " %d", cid_num);
			// strcat(reply, cid_num_str);

			int chunkSock;

			if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
				die(-1, "unable to create chunk socket\n");

			char* CID;

			while ((CID = strtok(NULL, " ")) != NULL) {
				// CIDs.push_back(strtok(NULL, " "));
				// printf("Prefetching chunk %d / %d\n", i+1, cid_num);
				ChunkStatus cs[NUM_CHUNKS]; // NUM_CHUNKS == 1 for now
				char data[XIA_MAXCHUNK];
				int len;
				int status;
				int n = -1;
				char *dag = (char *)malloc(512);
				char *cid = strtok(NULL, " ");
				sprintf(dag, "RE ( %s %s ) CID:%s", ftpServAD, ftpServHID, cid);
				cs[0].cidLen = strlen(dag);
				cs[0].cid = dag;
				unsigned ctr = 0;

				while (1) {

					if (ctr % REREQUEST == 0) {
						say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
						if (XrequestChunks(chunkSock, cs, n) < 0) {
								die(-1, "unable to request chunks\n");
						}
						say("checking chunk status\n");
					}
					ctr++;

					status = XgetChunkStatuses(chunkSock, cs, n);
					// TODO: optimize for enlarging the window size
					if (status == READY_TO_READ) {
						strcat(reply, " ");
						strcat(reply, cid);
						break;
					}
					else if (status < 0) {
						die(-1, "error getting chunk status\n");
					} 
					else if (status & WAITING_FOR_CHUNK) 
						say("waiting... one or more chunks aren't ready yet\n");
					else if (status & INVALID_HASH) 
						die(-1, "one or more chunks has an invalid hash");
					else if (status & REQUEST_FAILED)
						die(-1, "no chunks found\n");
					else 
						say("unexpected result\n");

					sleep(1); // TODO: investigate later
				}
				say("Chunk is ready\n");

				if ((len = XreadChunk(chunkSock, data, sizeof(data), 0, cs[0].cid, cs[0].cidLen)) < 0) {
					die(-1, "error getting chunk\n");
				}
				// TODO: need to check the next three lines: if the chunk is removed from the cache
				free(cs[0].cid);
				cs[0].cid = NULL;
				cs[0].cidLen = 0;
			}
			sendCmd(execProfileSock, reply); 	// chunk fetching ends
		}
	}

	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}
// execution section ends
*/

int main() {

	pthread_t thread_pred, thread_exec;
	pthread_create(&thread_pred, NULL, netMon, NULL);	// TODO: 
	pthread_create(&thread_exec, NULL, prefetchExec, NULL); // dequeue, prefetch and update profile

	ftpSock = initializeClient(ftp_name, myAD, myHID, ftpServAD, ftpServHID); // purpose is to get ftpServAD and ftpServHID for building chunk request

	profileServerSock = registerStreamReceiver(prefetch_profile_name, myAD, myHID, my4ID);
	blockListener((void *)&profileServerSock, clientReqCmd);
	return 0;	
/*
	// profile initilization starts
	profilePredSock = initializeClient(prefetch_pred_name, myAD, myHID, prefetchPredAD, prefetchPredHID);	
	profileServerSock = registerStreamReceiver(prefetch_profile_name, myAD, myHID, my4ID);
	blockListener((void *)&profileServerSock, profileDBCmd);
	// profile initilization ends

	// prediction initilization starts
	predProfileSock = initializeClient(prefetch_profile_name, myAD, myHID, prefetchProfileAD, prefetchProfileHID); 
	predExecSock = initializeClient(prefetch_exec_name, myAD, myHID, prefetchExecAD, prefetchExecHID);
	predServerSock = registerStreamReceiver(prefetch_pred_name, myAD, myHID, my4ID);
	blockListener((void *)&predServerSock, regCmd);	
	prefetch(predProfileSock, predExecSock);
	// prediction initilization ends

	// execution initilization starts
	ftpSock = initializeClient(ftp_name, myAD, myHID, ftpServAD, ftpServHID); // purpose is to get ftpServAD and ftpServHID for building chunk request
	execProfileSock = initializeClient(prefetch_profile_name, myAD, myHID, prefetchProfileAD, prefetchProfileHID);	
	execServerSock = registerStreamReceiver(prefetch_exec_name, myAD, myHID, my4ID);
	blockListener((void *)&execServerSock, execCmd);
	// execution initilization ends
*/

}
