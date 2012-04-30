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
#include <stdlib.h>
#include <errno.h>
#include "Xsocket.h"

#define VERSION "v1.0"
#define TITLE "XIA Chunk File Client"

#define DAG  "RE %s %s %s"
#define HID0 "HID:0000000000000000000000000000000000000000"
#define AD0   "AD:1000000000000000000000000000000000000000"
#define SID0 "SID:0f00000000000000000000000000000000009999"

#define NUM_CHUNKS	10

// global configuration options
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

int sendCmd(int sock, const char *cmd)
{
	int n;

	if ((n = Xsend(sock, cmd,  strlen(cmd), 0)) < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate with the server\n");
	}
	return n;
}

int getReply(int sock, char *reply, int sz)
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

int getFileData(int csock, FILE *fd, char *chunks)
{
	ChunkStatus cs[NUM_CHUNKS];
	char data[XIA_MAXCHUNK];
	char *p = chunks;
	char *next;
	int n = 0;
	int i;
	int len;
	int status;
	char *dag;

	// build the list of chunks to retrieve
	while ((next = strchr(p, ' '))) {
		*next = 0;

		dag = (char *)malloc(512);
		sprintf(dag, "RE ( %s %s ) CID:%s", AD0, HID0, p);
		printf("getting %s\n", p);
		cs[n].cidLen = strlen(dag);
		cs[n].cid = dag;
		n++;
		p = next + 1;
	}
	dag = (char *)malloc(512);
	sprintf(dag, "RE ( %s %s ) CID:%s", AD0, HID0, p);
	printf("getting %s\n", p);
	cs[n].cidLen = strlen(dag);
	cs[n].cid = dag;
	n++;

	// bring the list of chunks local
	printf("requesting list of %d chunks\n", n);
	if (XrequestChunks(csock, cs, n) < 0) {
		printf("unable to request chunks\n");
		return -1;
	}

	printf("checking chunk status\n");
	for (i = 0; i < 1000; i++) {
		status = XgetChunkStatuses(csock, cs, n);

status = 1;
		if (status == 1)
			break;

		else if (status < 0) {
			printf("error getting chunk status\n");
			return -1;

		} else if (status == 0) {
			// one or more chunks aren't ready.
			// in a real app we would want to wait to get chunks until they are ready
			// but we'll skip it in this demo code
			printf("one or more chunks aren't ready yet\n");
//			return -1;
		}
	}

	for (i = 0; i < n; i++) {
		char *cid = strrchr(cs[i].cid, ':');
		cid++;
		printf("reading chunk %s\n", cid);
		if ((len = XreadChunk(csock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
			printf("error getting chunk\n");
			return -1;
		}

		// write the chunk to disk
		printf("writing %d bytes of chunk %s to disk\n", len, cid);
		fwrite(data, 1, len, fd);
	}

	return n;
}

int main(int argc, char **argv)
{
	int sock, chunkSock;
	int offset;
	char *dag;
	const char *fname;
	char cmd[512];
	char reply[512];
	int status = 0;

	say ("\n%s (%s): started\n", TITLE, VERSION);

	if (argc != 2)
		die(-1, "usage: cftp <filename>\n");

	fname = argv[1];

	if (!(dag = createDAG(AD0, HID0, SID0)))
		die(-1, "unable to create DAG\n");

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(XSOCK_STREAM)) < 0)
		 die(-1, "Unable to create the listening socket\n");
    
	if (Xconnect(sock, dag) < 0) {
		Xclose(sock);
		 die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	// send the file request
	sprintf(cmd, "get %s",  fname);
	sendCmd(sock, cmd);

	// get back number of chunks in the file
	getReply(sock, reply, sizeof(reply));

	int count = atoi(&reply[4]);

	// FIXME: open a file here for writing the data to

	if ((chunkSock = Xsocket(XSOCK_CHUNK)) < 0)
		die(-1, "unable to create chunk socket\n");

	printf("opening output file\n");
	FILE *f = fopen("foo", "w");

	offset = 0;
	while (offset < count) {
		int num = NUM_CHUNKS;
		if (count - offset < num)
			num = count - offset;

		// tell the server we want a list of <num> cids starting at location <offset>
		sprintf(cmd, "block %d:%d", offset, num);
		sendCmd(sock, cmd);

		getReply(sock, reply, sizeof(reply));
		printf("%s\n", reply);
		offset += NUM_CHUNKS;

		if (getFileData(chunkSock, f, &reply[4]) < 0) {
			status= -1;
			break;
		}
	}
	
	// FIXME: close the file
	printf("closing output file\n");
	fclose(f);

	if (status < 0) {
		// unlink the file
		printf("unlinking the output file\n");
	}

	printf("shutting down\n");
	sendCmd(sock, "done");
	Xclose(sock);
	Xclose(chunkSock);
	return status;
}
