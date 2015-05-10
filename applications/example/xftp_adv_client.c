#include "prefetch_utils.h"

#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP client"

using namespace std;

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];

char ftpAD[MAX_XID_SIZE];
char ftpHID[MAX_XID_SIZE];

char prefetchClientAD[MAX_XID_SIZE];
char prefetchClientHID[MAX_XID_SIZE];

char prefetchProfileAD[MAX_XID_SIZE];
char prefetchProfileHID[MAX_XID_SIZE];

char *ftp_name = "www_s.ftp.advanced.aaa.xia";
char *prefetch_client_name = "www_s.client.prefetch.aaa.xia";
char *prefetch_profile_name = "www_s.profile.prefetch.aaa.xia";

int ftpSock;
int prefetchClientSock;
int	prefetchProfileSock;

int getFile(int sock, char *p_ad, char *p_hid, const char *fin, const char *fout) {
	int chunkSock;
	int offset;
	char cmd[XIA_MAX_BUF];
	char reply[XIA_MAX_BUF];
	int n = -1;
	int status = 0;
	char prefetch_cmd[XIA_MAX_BUF];

	// send the file request to the xftp server
	sprintf(cmd, "get %s", fin);
	sendCmd(sock, cmd);

	if ((n = Xrecv(sock, reply, sizeof(reply), 0))  < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate with the server\n");
	}
	char reply_arr[strlen(reply)];
	strcpy(reply_arr, reply);
	char *pch;
	int cid_num = atoi(strtok(reply_arr, " "));
	vector<string> CIDs;
	for (int i = 0; i < cid_num; i++) {
		pch = strtok(NULL, " ");		
		CIDs.push_back(pch);
	}
	for (int i = 0; i < cid_num; i++) {
		cout<<CIDs[i]<<endl;
	}
	// TODO: get SID and append
/*		
	// get back number of chunks in the file
	if (getChunkCount(sock, reply, sizeof(reply)) < 1){
		warn("could not get chunk count. Aborting. \n");
		return -1;
	}

	int count = atoi(&reply[4]); 	// reply: OK: ***

	say("%d chunks in total\n", count);

	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");

	FILE *f = fopen(fout, "w");

	offset = 0;

	struct timeval tv;
	int start_msec = 0;
	int temp_start_msec = 0;
	int temp_end_msec = 0;
	
	if (gettimeofday(&tv, NULL) == 0)
		start_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
			
	while (offset < count) {
		int num = NUM_CHUNKS;
		if (count - offset < num)
			num = count - offset;

		// tell the server we want a list of <num> cids starting at location <offset>
		printf("\nFetched chunks: %d/%d:%.1f%\n\n", offset, count, 100*(double)(offset)/count);

		sprintf(cmd, "block %d:%d", offset, num);
		
		if (gettimeofday(&tv, NULL) == 0)
			temp_start_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
		// TODO: embedded its SID as well!					
		// send the requested CID range
		sendCmd(sock, cmd);
*/
		// TODO: send file name and current chunks 
//		sprintf(prefetch_cmd, "get %s %d %d", fin, offset, offset + NUM_CHUNKS - 1);
//		int m = sendCmd(prefetch_ctx_sock, prefetch_cmd); 
/*
		char* hello = "Hello from context client";
		int m = sendCmd(prefetch_ctx_sock, hello); 
		printf("%d\n");
		say("Sent CID update\n");
*/

/*		
		if (getChunkCount(sock, reply, sizeof(reply)) < 1) {
			warn("could not get chunk count. Aborting. \n");
			return -1;
		}
		offset += NUM_CHUNKS;
		// &reply[4] are the requested CIDs  
		if (getListedChunks(chunkSock, f, &reply[4], p_ad, p_hid) < 0) {
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
*/	
	Xclose(chunkSock);
	return status;
}

int main(int argc, char **argv) {	

	char fin[512], fout[512];
	char cmd[512], reply[512];
	int params = -1;

	say ("\n%s (%s): started\n", TITLE, VERSION);
	
	ftpSock = initializeClient(ftp_name, myAD, myHID, ftpAD, ftpHID);
/*	
	prefetchClientSock = initializeClient(prefetch_client_name, myAD, myHID, prefetchClientAD, prefetchClientHID);
	prefetchProfileSock = initializeClient(prefetch_profile_name, myAD, myHID, prefetchProfileAD, prefetchProfileHID);

	char* hello = "Hello from ftp client";		
	sayHello(prefetchClientSock, hello);
	sayHello(prefetchProfileSock, hello);
	say("Say hello to prefetch client and prefetch profile.\n");
*/

	usage();

	while (1) {
		say(">>");
		cmd[0] = '\n';
		fin[0] = '\n';
		fout[0] = '\n';
		params = -1;
		
		fgets(cmd, 511, stdin); // read input into cmd

		// compare the first three characters
		if (strncmp(cmd, "get", 3) == 0) {
			// params is the number of arguments
			params = sscanf(cmd, "get %s %s", fin, fout);
			if (params != 2) {
				sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
				warn(reply);
				usage();
				continue;
			}
			if (strcmp(fin, fout) == 0) {
				warn("Name of fin and fout should be different.\n");
				continue;
			}
			getFile(ftpSock, ftpAD, ftpHID, fin, fout);
		}
		else {
			sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
			warn(reply);
			usage();
		}
	}
	return 1;
}
