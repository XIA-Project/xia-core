#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Prefetch Executer"

using namespace std;

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char prefetchPredAD[MAX_XID_SIZE];
char prefetchPredHID[MAX_XID_SIZE];

char prefetch_profile_name[] = "www_s.profile.prefetch.aaa.xia";
char prefetch_pred_name[] = "www_s.prediction.prefetch.aaa.xia";

int prefetchProfileSock, prefetchPredSock;

pthread_mutex_t profileLock = PTHREAD_MUTEX_INITIALIZER;

#define BLANK 0; // initilized
#define REQ 1; // requested by router
#define DONE 2; // available in cache

struct chunkStatus {
	char *CID;
	long timestamp; // msec
	bool fetch;
	int prefetch;
};

map<string, vector<chunkStatus> > profile;

void xftp_client_reg_handler(int sock, char *cmd) {
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

	// let prefetch_pred know a new SID to keep track	
	char reply[strlen(SID)+4];
	strcat(reply, "sid ");
	strcat(reply, SID);
	sendCmd(prefetchPredSock, reply);
}

void xftp_client_fetch_handler(int sock, char *cmd) {

	char cmd_arr[strlen(cmd)];
	strcpy(cmd_arr, cmd);
	char *SID = XgetRemoteSID(sock);
	strtok(cmd_arr, " "); // skip the "fetch"
	char *CID = strtok(NULL, " ");

	char reply[XIA_MAX_BUF];
	// reply format: available cid1 cid2, ...
	strcat(reply, "available");
	pthread_mutex_lock(&profileLock);
	vector<chunkStatus> css = profile[SID];

	// return the CIDs which chunks are available 
	for (unsigned int i = 0; i < css.size(); i++) {
		if (strcmp(css[i].CID, CID) == 0) {
			unsigned j = i;
			while (1) {
				// mark fetchByClient as "true" when the chunk is available after probe				
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
	pthread_mutex_unlock(&profileLock);
	sendCmd(sock, reply);
}

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
	/*
	for (int i = 0; i < cid_num; i++) {
		for (unsigned int j = 0; j < css.size(); j++) {
			if (strcmp(css[j].CID, strtok(NULL, " ")) == 0) 
				css[j].prefetch = DONE;
		}
	}*/
  pthread_mutex_unlock(&profileLock);
}

void *profileDBCmd (void *socketid) {

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
			xftp_client_reg_handler(sock, cmd);
		}
		// fetch probe from xftp client: fetch CID
		else if (strncmp(cmd, "fetch", 5) == 0) {
			xftp_client_fetch_handler(sock, cmd);
		}
		// prefetch request from prefetch pred
		else if (strncmp(cmd, "prefetch", 8) == 0) {
			prefetch_pred_handler(sock, cmd);
		}
		// chunk available update from prefetch exec
		else if (strncmp(cmd, "prefetched", 10) == 0) {
			prefetch_exec_handler(sock, cmd);
		}
		/*
		for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
			for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
				cout<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).reqByClient<<"\t"<<(*J).prefetch<<endl;
			}
		}	
		*/
	}
	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

int main() {

	prefetchPredSock = initializeClient(prefetch_pred_name, myAD, myHID, prefetchPredAD, prefetchPredHID);	
	prefetchProfileSock = registerStreamReceiver(prefetch_profile_name, myAD, myHID, my4ID);
	blockListener((void *)&prefetchProfileSock, profileDBCmd);

	return 0;
}
