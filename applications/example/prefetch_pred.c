#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Prefetch Prediction"

using namespace std;

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

char prefetchExecAD[MAX_XID_SIZE];
char prefetchExecHID[MAX_XID_SIZE];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char prefetch_profile_name[] = "www_s.profile.prefetch.aaa.xia";
char prefetch_pred_name[] = "www_s.prediction.prefetch.aaa.xia";
char prefetch_exec_name[] = "www_s.executer.prefetch.aaa.xia";

int	prefetchProfileSock, prefetchPredSock, prefetchExecSock;

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
int prefetch(int prefetchProfileSock, int prefetchExecSock) {

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
			sendCmd(prefetchProfileSock, cmd);
			if ((n = Xrecv(prefetchProfileSock, reply, sizeof(reply), 0))  < 0) {
				warn("socket error while waiting for data, closing connection\n");
				break;
			}
			// forward the CID to prefetch to prefetch executer if any
			if (strncmp(reply, "prefetch", 8) == 0 && strncmp(reply, "prefetch none", 13) != 0) {
				sendCmd(prefetchExecSock, reply);
			}
		}
		usleep(100000); // TODO: investigate 
	}
	return -1;
}

int main() {

	prefetchProfileSock = initializeClient(prefetch_profile_name, myAD, myHID, prefetchProfileAD, prefetchProfileHID); 
	prefetchExecSock = initializeClient(prefetch_exec_name, myAD, myHID, prefetchExecAD, prefetchExecHID);
	prefetchPredSock = registerStreamReceiver(prefetch_pred_name, myAD, myHID, my4ID);
	blockListener((void *)&prefetchPredSock, regCmd);	
	prefetch(prefetchProfileSock, prefetchExecSock);
}
