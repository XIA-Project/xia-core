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

#define VERSION "v1.0"
#define TITLE "XIA Chunk File Server"

#define DAG  "RE %s %s %s"
#define AD0  "AD:1000000000000000000000000000000000000000"
#define HID0 "HID:0000000000000000000000000000000000000000"
#define SID0 "SID:0f00000000000000000000000000000000009999"

#define CHUNKSIZE 8192

int verbose = 1;

/*
** simple code to create a formatted DAG
**
** The dag should be free'd by the calling code when no longer needed
*/
char *createDAG(const char *ad, const char *host, const char *service)
{
	int len = snprintf(NULL, 0, DAG, ad, host, service) + 1;
    char * dag = (char*)malloc(len);
    sprintf(dag, DAG, ad, host, service);
	return dag;
}

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

void *processRequest (void *socketid)
{
	int i, n, count;
	ChunkInfo *info = NULL;
	char command[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	char *fname;

	ChunkContext *ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 20000000);
	if (ctx == NULL)
		die(-2, "Unable to initilize the chunking system\n");

	while (1) {
		say("waiting for command\n");
		if ((n = Xrecv(sock, command, 1024, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		if (strncmp(command, "get", 3) == 0) {
			fname = &command[4];
			say("client requested file %s\n", fname);

			if (info) {
				// clean up the existing chunks first
			}
		
			info = NULL;
			count = 0;
			
			say("chunking file %s\n", fname);

			if ((count = XputFile(ctx, fname, CHUNKSIZE, &info)) < 0) {
				warn("unable to serve the file: %s\n", fname);
				sprintf(reply, "FAIL: File (%s) not found", fname);

			} else {
				sprintf(reply, "OK: %d", count);
			}
			say("%s\n", reply);

			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}



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
			for (int i = 0; i < count; i++)
				XremoveChunk(ctx, info[i].cid);
			XfreeChunkInfo(info);
			info = NULL;
			count = 0;
			break;
		
		} else {
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

/*
** where it all happens
*/
int main()
{
	char *dag;
	int sock, acceptSock;

	say ("\n%s (%s): started\n", TITLE, VERSION);

	if (!(dag = createDAG(AD0, HID0, SID0))) {
		die(-1, "unable to create DAG\n");
	}

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(XSOCK_STREAM)) < 0)
		 die(-1, "Unable to create the listening socket\n");
    
	if (Xbind(sock,dag) < 0) {
		Xclose(sock);
		 die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	say("listening on dag: %s\n", dag);

	free(dag);

   	while (1) {
		say("Waiting for a client connection\n");
   		
		if ((acceptSock = Xaccept(sock)) < 0)
			die(-1, "accept failed\n");

		say("connected\n");
		
		// handle the connection in a new thread
		pthread_t client;
       	pthread_create(&client, NULL, processRequest, (void *)&acceptSock);
	}
	
	Xclose(sock); // we should never reach here!
	
	return 0;
}
