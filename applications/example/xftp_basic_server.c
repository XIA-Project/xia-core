/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include <assert.h>

#define VERSION "v1.0"
#define TITLE "XIA Basic FTP Server"

#define MAX_XID_SIZE 100
// #define DAG  "RE %s %s %s"
#define SID "SID:00000000dd41b924c1001cfa1e1117a812492434"
#define NAME "www_s.basicftp.aaa.xia"

#define CHUNKSIZE 1024
#define NUM_CHUNKS	10
#define REREQUEST 3

int verbose = 1;
char myAD[MAX_XID_SIZE];
char myHID[MAX_XID_SIZE];
char my4ID[MAX_XID_SIZE];

int getFile(int sock, char *ad, char*hid, const char *fin, const char *fout);


/*
** write the message to stdout unless in quiet mode
*/
void say(const char *fmt, ...)
{
	if (verbose) {
		va_list args;

		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
}

/*
** always write the message to stdout
*/
void warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);

}

/*
** write the message to stdout, and exit the app
*/
void die(int ecode, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);
	fprintf(stdout, "%s: exiting\n", TITLE);
	exit(ecode);
}

int sendCmd(int sock, const char *cmd)
{
	int n;
	warn("Sending Command: %s \n", cmd);

	if ((n = Xsend(sock, cmd,  strlen(cmd), 0)) < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate\n");
	}

	return n;
}

char** str_split(char* a_str, const char *a_delim)
{
	char** result    = 0;
	int count     = 0;
	int str_len = strlen(a_str);
	int del_len = strlen(a_delim);
	int i = 0;
	int j = 0;
	char* last_delim = 0;

	/* Count how many elements will be extracted. */
	for(i = 0 ; i < str_len; i++) 
		for(j = 0 ; j < del_len; j++) 
			if( a_str[i] == a_delim[j]){
				count++;
				last_delim = &a_str[i];
			}

	
	 /* Add space for trailing token. */
	count += last_delim < (a_str + strlen(a_str) - 1);
	
// 	/* Add space for terminating null string so caller
// 	knows where the list of returned strings ends. */
 	count++;

	result = (char **) malloc(sizeof(char*) * count);
	
// 	printf ("Splitting string \"%s\" into %i tokens:\n", a_str, count);
	
	i = 0;
	result[i] = strtok(a_str, a_delim);
// 	printf ("%s\n",result[i]);
	
	for( i = 1; i < count; i++){
		result[i] = strtok (NULL, a_delim);
// 		printf ("%s\n",result[i]);
	}

	return result;
}

void *recvCmd (void *socketid)
{
	int i, n, count = 0;
	ChunkInfo *info = NULL;
	char command[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	char *fname;
	char fin[512], fout[512];
//  	char **params;
	char ad[MAX_XID_SIZE];
	char hid[MAX_XID_SIZE];

	//ChunkContext contains size, ttl, policy, and contextID which for now is PID
	ChunkContext *ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 20000000);
	if (ctx == NULL)
		die(-2, "Unable to initilize the chunking system\n");

	while (1) {
		say("waiting for command\n");
		
		memset(command, '\0', strlen(command));
		memset(reply, '\0', strlen(reply));
		
		if ((n = Xrecv(sock, command, 1024, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		//Sender does the chunking and then should start the sending commands
		if (strncmp(command, "get", 3) == 0) {
			fname = &command[4];
			say("client requested file %s\n", fname);

			if (info) {
				// clean up the existing chunks first
			}
		
			info = NULL;
			count = 0;
			
			say("chunking file %s\n", fname);
			
			//Chunking is done by the XputFile which itself uses XputChunk, and fills out the info
			if ((count = XputFile(ctx, fname, CHUNKSIZE, &info)) < 0) {
				warn("unable to serve the file: %s\n", fname);
				sprintf(reply, "FAIL: File (%s) not found", fname);

			} else {
				sprintf(reply, "OK: %d", count);
			}
			say("%s\n", reply);
			
			
			//Just tells the receiver how many chunks it should expect.
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		//Sending the blocks cids
		} else if (strncmp(command, "block", 5) == 0) {
			char *start = &command[6];
			char *end = strchr(start, ':');
			if (!end) {
				// we have an invalid command, return error to client
				sprintf(reply, "FAIL: invalid block command");
			} else {
				*end = 0;
				end++;
				// FIXME: clean up naming, e is now a count
				int s = atoi(start);
				int e = s + atoi(end);
				strcpy(reply, "OK:");
				for(i = s; i < e && i < count; i++) {
					strcat(reply, " ");
					strcat(reply, info[i].cid);
				}
			}
			printf("%s\n", reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}

		} else if (strncmp(command, "done", 4) == 0) {
			say("done sending file: removing the chunks from the cache\n");
			for (i = 0; i < count; i++)
				XremoveChunk(ctx, info[i].cid);
			XfreeChunkInfo(info);
			info = NULL;
			count = 0;
		
		}
		else if (strncmp(command, "put", 3) == 0) {
// 			fname= &command[4];
			i = 0;
			say("Client wants to put a file here. cmd: %s\n" , command);
//			put %s %s %s %s 
			i = sscanf(command,"put %s %s %s %s ",ad,hid,fin,fout );
			if (i != 4){
 				warn("Invalid put command: %s\n" , command);
			}
			getFile(sock, ad, hid, fin, fout);

		}else {
			sprintf(reply, "FAIL: invalid command (%s)\n", command);
			warn(reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		}
	}
	
	if (info)
		XfreeChunkInfo(info);
	XfreeCacheSlice(ctx);
	Xclose(sock);
	pthread_exit(NULL);
}


//Just registering the service and openning the necessary sockets
int registerReceiver()
{
    int sock;
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];

	say ("\n%s (%s): started\n", TITLE, VERSION);

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		 die(-1, "Unable to create the listening socket\n");

    // read the localhost AD and HID
    if ( XreadLocalHostAddr(sock, myAD, sizeof(myAD), myHID, sizeof(myHID), my4ID, sizeof(my4ID)) < 0 )
    	die(-1, "Reading localhost address\n");

    // Generate an SID to use
    if(XmakeNewSID(sid_string, sizeof(sid_string))) {
        die(-1, "Unable to create a temporary SID");
    }

    struct addrinfo hints, *ai;
    bzero(&hints, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_XIA;
    if (Xgetaddrinfo(NULL, sid_string, &hints, &ai) != 0)
        die(-1, "getaddrinfo failure!\n");

    Graph g((sockaddr_x*)ai->ai_addr);

    sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;
	if (Xbind(sock, (struct sockaddr*)sa, sizeof(sockaddr_x)) < 0) {
		Xclose(sock);
		 die(-1, "Unable to bind to the dag: %s\n", g.dag_string().c_str());
	}
	say("listening on dag: %s\n", g.dag_string().c_str());

	Xlisten(sock, 5);

	//FIXME NAME is hard coded
    if (XregisterName(NAME, sa) < 0 )
        die(-1, "error registering name: %s\n", NAME);
    say("\nRegistering DAG with nameserver:\n%s\n", g.dag_string().c_str());

  return sock;
  
}

void *blockingListener(void *socketid)
{
  int sock = *((int*)socketid);
  int acceptSock;
  while (1) {
		say("Waiting for a client connection\n");
   		
		if ((acceptSock = Xaccept(sock, NULL, NULL)) < 0)
			die(-1, "accept failed\n");

		say("connected\n");
		
		// handle the connection in a new thread
		pthread_t client;
	pthread_create(&client, NULL, recvCmd, (void *)&acceptSock);
	}
	
	Xclose(sock); // we should never reach here!
	return NULL;
}

// not used
void nonblockingListener(int sock)
{
	pthread_t client;
       	pthread_create(&client, NULL, blockingListener, (void *)&sock);
  
}

int getChunkCount(int sock, char *reply, int sz)
{
	int n;

	if ((n = Xrecv(sock, reply, sz, 0))  < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate with the server\n");
	}

	if (strncmp(reply, "OK:", 3) != 0) {
		die(-1, "%s\n", reply);
	}

	reply[n] = 0;

	return n;
}


int buildChunkDAGs(ChunkStatus cs[], char *chunks, char *ad, char *hid)
{
	char *p = chunks;
	char *next;
	int n = 0;

	char *dag;
	
	
	// build the list of chunks to retrieve
	while ((next = strchr(p, ' '))) {
		*next = 0;

		dag = (char *)malloc(512);
		sprintf(dag, "RE ( %s %s ) CID:%s", ad, hid, p);
// 		printf("built dag: %s\n", dag);
		cs[n].cidLen = strlen(dag);
		cs[n].cid = dag;
		n++;
		p = next + 1;
	}
	dag = (char *)malloc(512);
	sprintf(dag, "RE ( %s %s ) CID:%s", ad, hid, p);
// 	printf("built dag: %s\n", dag);
	cs[n].cidLen = strlen(dag);
	cs[n].cid = dag;
	n++;
	return n;
}

int getListedChunks(int csock, FILE *fd, char *chunks, char *ad, char *hid)
{
	ChunkStatus cs[NUM_CHUNKS];
	char data[XIA_MAXCHUNK];
	int len;
	int status;
	int n = -1;
	
	
	n = buildChunkDAGs(cs, chunks, ad, hid);
	
	// NOTE: the chunk transport is not currently reliable and chunks may need to be re-requested
	// ask for the current chunk list again every REREQUEST seconds
	// chunks already in the local cache will not be refetched from the network 
	unsigned ctr = 0;
	while (1) {
		if (ctr % REREQUEST == 0) {
			// bring the list of chunks local
			say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
			if (XrequestChunks(csock, cs, n) < 0) {
				say("unable to request chunks\n");
				return -1;
			}
			say("checking chunk status\n");
			ctr++;
		}

		status = XgetChunkStatuses(csock, cs, n);

		if (status == READY_TO_READ)
			break;

		else if (status < 0) {
			say("error getting chunk status\n");
			return -1;

		} else if (status & WAITING_FOR_CHUNK) {
			// one or more chunks aren't ready.
			say("waiting... one or more chunks aren't ready yet\n");
		
		} else if (status & INVALID_HASH) {
			die(-1, "one or more chunks has an invalid hash");
		
		} else if (status & REQUEST_FAILED) {
			die(-1, "no chunks found\n");

		} else {
			say("unexpected result\n");
		}
		sleep(1);
	}

	say("all chunks ready\n");

	for (int i = 0; i < n; i++) {
		char *cid = strrchr(cs[i].cid, ':');
		cid++;
		say("reading chunk %s\n", cid);
		if ((len = XreadChunk(csock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
			say("error getting chunk\n");
			return -1;
		}

		// write the chunk to disk
//		say("writing %d bytes of chunk %s to disk\n", len, cid);
		fwrite(data, 1, len, fd);

		free(cs[i].cid);
		cs[i].cid = NULL;
		cs[i].cidLen = 0;
	}

	return n;
}


//	This is used both to put files and to get files since in case of put I still have to request the file.
//	Should be fixed with push implementation

int getFile(int sock, char *ad, char*hid, const char *fin, const char *fout)
{
	int chunkSock;
	int offset;
	char cmd[512];
	char reply[512];
	int status = 0;
	
	//TODO: check the arguments to be correct
	
	say("Getting file: %s, write to: %s \n",fin, fout);
	
	// send the file request
	sprintf(cmd, "get %s",  fin);
	sendCmd(sock, cmd);

	// get back number of chunks in the file
	if (getChunkCount(sock, reply, sizeof(reply)) < 1){
		warn("could not get chunk count. Aborting. \n");
		return -1;
	}	

	int count = atoi(&reply[4]);

	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");

	FILE *f = fopen(fout, "w");

	offset = 0;
	while (offset < count) {
		int num = NUM_CHUNKS;
		if (count - offset < num)
			num = count - offset;

		// tell the server we want a list of <num> cids starting at location <offset>
		sprintf(cmd, "block %d:%d", offset, num);
		sendCmd(sock, cmd);

		if (getChunkCount(sock, reply, sizeof(reply)) < 1){
			warn("could not get chunk count. Aborting. \n");
			return -1;
		}
		offset += NUM_CHUNKS;
		if (getListedChunks(chunkSock, f, &reply[4], ad, hid) < 0) {
			status= -1;
			break;
		}
	}
	
	fclose(f);

	if (status < 0) {
		unlink(fin);
	}

	say("File Transfer Complete. \n");
	sendCmd(sock, "done");
	Xclose(chunkSock);
	return status;
}


int main()
{	
	int sock = registerReceiver();
	blockingListener((void *)&sock);
	return 0;
}
