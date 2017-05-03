#include "stage_utils.h"
#include <stdlib.h> 
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP Server"

char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];
int CHUNKSIZE = 512 * 1024;
void getConfig(int argc, char** argv)
{
        int c;
        opterr = 0;

        while ((c = getopt(argc, argv, "c:")) != -1) {
                switch (c) {
                        case 'c':
                        	if ((CHUNKSIZE = atoi(optarg)) != 0) {
                            	CHUNKSIZE *= 1024;
                       		}
                        	break;
                        
                        default:
                                break;
                }
        }
}

void *recvCmd (void *socketid) 
{
	int i, n, count = 0;
	char cmd[XIA_MAX_BUF + 4];
	char reply[XIA_MAX_BUF + 5];
	int sock = *((int*)socketid);
	char *fname;
	sockaddr_x *info = NULL;

	XcacheHandle xcache;
	XcacheHandleInit(&xcache);
	// ChunkContext contains size, ttl, policy, and contextID which for now is PID

	while (1) {
		say("waiting for command\n");
		memset(cmd, '\0', strlen(cmd));
		memset(reply, '\0', strlen(reply));
		
		if ((n = Xrecv(sock, cmd, XIA_MAX_BUF, 0)) < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
say("Receive cmd: %s\n", cmd);
		if (strncmp(cmd, "get", 3) == 0) {
			fname = &cmd[4];
			say("Client requested file %s\n", fname);

			
			say("Chunking file %s\n", fname);
			
			// Chunking is done by the XputFile which itself uses XputChunk, and fills out the info
			if ((count = XputFile(&xcache, fname, CHUNKSIZE, &info)) < 0) {
				warn("unable to serve the file: %s\n", fname);
				sprintf(reply, "FAIL: File (%s) not found", fname);
			} 

			else {
				int offset = 0;
				int num;
				// Each CID's size is set to MAX_CID_NUM
				while (offset < count) {
					num = MAX_CID_NUM;
					if (count - offset < MAX_CID_NUM) {
						num = count - offset;
					}
					memset(reply, '\0', strlen(reply));
					sprintf(reply, "cont");
					char url[256];
					for (i = offset; i < offset + num; i++) {
						//dag_to_url(url, 256, &info[i]);
						Graph g(&info[i]);
						g.fill_sockaddr(&info[i]);
						strcpy(url, g.http_url_string().c_str());
						
						strcat(reply, " ");
						strcat(reply, url);
					}
					offset += MAX_CID_NUM;
					// Send CID list to client.
					say("Sending %d chunks of %d to %d: %s\n", num, offset, offset + num, reply);
					if (Xsend(sock, reply, strlen(reply), 0) < 0) {
						warn("unable to send reply to client\n");
						break;
					}
					hearHello(sock);
				}
				memset(reply, '\0', strlen(reply));
				sprintf(reply, "done");	
				if (Xsend(sock, reply, strlen(reply), 0) < 0) {
					warn("unable to send reply to client\n");
					break;
				}								
			}
		}
		// After all chunks had been sent, they can be removed from server. 	
		else if (strncmp(cmd, "done", 4) == 0) {
			say("done sending file: removing the chunks from the cache\n");
			free(info);
			info = NULL;
			count = 0;
		} 
		else {
			sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
			warn(reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		}
	}
	
	if (info) {
		free(info);
		info = NULL;
	}
	Xclose(sock);
	pthread_exit(NULL);
}

int main(int argc, char **argv) 
{
	getConfig(argc, argv);	
	sockaddr_x *info = NULL;
	XcacheHandle xcache;
	XcacheHandleInit(&xcache);
	XputFile(&xcache, "11", CHUNKSIZE, &info);
	XputFile(&xcache, "22", CHUNKSIZE, &info);
	XputFile(&xcache, "33", CHUNKSIZE, &info);
	XputFile(&xcache, "44", CHUNKSIZE, &info);
	XputFile(&xcache, "55", CHUNKSIZE, &info);
	int ftpListenSock = registerStreamReceiver(getXftpName(), myAD, myHID, my4ID);
	blockListener((void *)&ftpListenSock, recvCmd);
	return 0;
}
