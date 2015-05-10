#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Prefetch Executer"

using namespace std;

char src_ad[MAX_XID_SIZE];
char src_hid[MAX_XID_SIZE];

char dst_ad[MAX_XID_SIZE];
char dst_hid[MAX_XID_SIZE];

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

char* prefetch_client_name = "www_s.client.prefetch.aaa.xia";
char* prefetch_profile_name = "www_s.profile.prefetch.aaa.xia";
char* prefetch_pred_name = "www_s.prediction.prefetch.aaa.xia";
char* prefetch_exec_name = "www_s.executer.prefetch.aaa.xia";

int	ftpClientSock;
int prefetchClientSock;
int prefetchPredSock;
int prefetchExecSock;

struct chunkStatus {
	string CID;
	long long timestamp;
	int status; // 1: requested by client; 2: requested by router; 3: available
};

map<string, vector<chunkStatus> > profile;

int getSubFile(int sock, char *dst_ad, char* dst_hid, const char *fin, const char *fout, int start, int end);

// prefetch_client receives context updates and send predicted CID_s to prefetch_server
// handle the CID update first and location/AP later
void *RecvCmd (void *socketid) {
	char command[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	int n;
	char fin[512];
	char fout[512];
	int start, end;

	while (1) {
		memset(command, '\0', strlen(command));
		memset(reply, '\0', strlen(reply));

		if ((n = Xrecv(sock, command, 1024, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		// printf("%d\n", n);
		/*
		if (strncmp(command, "Hello from prefetch client", 26) == 0)
			say("Received hello from prefetch client\n");
		*/
		printf("%s\n", command);
		// TODO: use XChunk to prefetch: establish connection and start from the next 10 chunks
		if (strncmp(command, "get", 3) == 0) {
			sscanf(command, "get %s %d %d", fin, &start, &end);
			printf("get %s %d %d\n", fin, start, end);
			// getSubFile(sockfd_ftp, dst_ad, dst_hid, fin, fin, start, end); // TODO: why fin, fin?
		}
	}
/*
	chunkStatus cs;
	cs.CID = "1234567890";
	cs.timestamp = 11111111;
	cs.status = 3;
	vector<chunkStatus> CIDs; 
	CIDs.push_back(cs);
	CIDs.push_back(cs);

	profile["12123212"] = CIDs;
	for (map<string, vector<chunkStatus> >::iterator I = profile.begin(); I != profile.end(); ++I) {
		for (vector<chunkStatus>::iterator J = (*I).second.begin(); J != (*I).second.end(); ++J) {
			cout<<(*I).first<<"\t"<<(*J).CID<<"\t"<<(*J).timestamp<<"\t"<<(*J).status<<endl;
		}
	}	
	*/
	Xclose(sock);
	say("Socket closed\n");
	pthread_exit(NULL);
}

int getSubFile(int sock, char *dst_ad, char* dst_hid, const char *fin, const char *fout, int start, int end) {
	int chunkSock;
	int offset;
	char cmd[512];
	char reply[512];
	int status = 0;
	
	// send the file request
	sprintf(cmd, "get %s", fin);
	sendCmd(sock, cmd);

	// get back number of chunks in the file
	if (getChunkCount(sock, reply, sizeof(reply)) < 1){
		warn("could not get chunk count. Aborting. \n");
		return -1;
	}

	// reply: OK: ***
	int count = atoi(&reply[4]);

	say("%d chunks in total\n", count);

	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");

	FILE *f = fopen(fout, "w");

	offset = start;

	struct timeval tv;
	int start_msec, temp_start_msec, temp_end_msec;
	
	if (gettimeofday(&tv, NULL) == 0)
		start_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
			
	// TODO: when to end?
	while (offset < count) {
		int num = NUM_CHUNKS;
		if (count - offset < num)
			num = count - offset;

		// tell the server we want a list of <num> cids starting at location <offset>
		printf("\nFetched chunks: %d/%d:%.1f%\n\n", offset, count, 100*(double)(offset)/count);

		sprintf(cmd, "block %d:%d", offset, num);
		
		if (gettimeofday(&tv, NULL) == 0)
			temp_start_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
					
		// send the requested CID range
		sendCmd(sock, cmd);
/*
		// say hello to the connext server
		char* hello = "Hello from context client";		
		int m = sendCmd(prefetch_ctx_sock, hello); 
		printf("%d\n");
		say("Sent hello msg\n");
*/
		if (getChunkCount(sock, reply, sizeof(reply)) < 1) {
			warn("could not get chunk count. Aborting. \n");
			return -1;
		}
		offset += NUM_CHUNKS;
		// &reply[4] are the requested CIDs  
		if (getListedChunks(chunkSock, f, &reply[4], dst_ad, dst_hid) < 0) {
			status= -1;
			break;
		}
		if (gettimeofday(&tv, NULL) == 0)
			temp_end_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
					
		printf("Time elapses: %.3f seconds\n", (double)(temp_end_msec - temp_start_msec)/1000);
	}
	printf("Time elapses: %.3f seconds in total.\n", (double)(temp_end_msec - start_msec)/1000);	
	fclose(f);

	if (status < 0) {
		unlink(fin);
	}

	say("Received file %s\n", fout);
	sendCmd(sock, "done");
	Xclose(chunkSock);
	return status;
}

int main() {

	prefetchClientSock = initializeClient(prefetch_client_name, src_ad, src_hid, dst_ad, dst_hid); 
	ftpClientSock = registerStreamReceiver(prefetch_pred_name, myAD, myHID, my4ID);
	// response to query
	blockListener((void *)&prefetchClientSock, RecvCmd);

	//getSubFile(sockfd_ftp, s_ad, s_hid, "4.png", "my4.png", 0, 1);
}
