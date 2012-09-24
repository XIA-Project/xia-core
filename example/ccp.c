/* ts=4 */
/*
** Copyright 2012 Carnegie Mellon University
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
#define NAME "www_s.chunkcopy.aaa.xia"

#define NUM_CHUNKS	10

// global configuration options
int verbose = 1;

char *ad;
char *hid;

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
		sprintf(dag, "RE ( %s %s ) CID:%s", ad, hid, p);
//		printf("getting %s\n", p);
		cs[n].cidLen = strlen(dag);
		cs[n].cid = dag;
		n++;
		p = next + 1;
	}
	dag = (char *)malloc(512);
	sprintf(dag, "RE ( %s %s ) CID:%s", ad, hid, p);
//	printf("getting %s\n", p);
	cs[n].cidLen = strlen(dag);
	cs[n].cid = dag;
	n++;

	// bring the list of chunks local
	say("requesting list of %d chunks\n", n);
	if (XrequestChunks(csock, cs, n) < 0) {
		say("unable to request chunks\n");
		return -1;
	}

	say("checking chunk status\n");
	while (1) {
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

	for (i = 0; i < n; i++) {
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

int main(int argc, char **argv)
{
	int sock, chunkSock;
	int offset;
	char *dag;
	char *p;
	const char *fin;
	const char *fout;
	char cmd[512];
	char reply[512];
	int status = 0;

	say ("\n%s (%s): started\n", TITLE, VERSION);

	if (argc != 3)
		die(-1, "usage: cftp <source file> <dest file>\n");

	fin = argv[1];
	fout = argv[2];

    // lookup the xia service 
    if (!(dag = XgetDAGbyName(NAME)))
		die(-1, "unable to locate: %s\n", NAME);


	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(XSOCK_STREAM)) < 0)
		 die(-1, "Unable to create the listening socket\n");
    
	if (Xconnect(sock, dag) < 0) {
		Xclose(sock);
		 die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	// save the AD and HID for later. This seems hacky
	// we need to find a better way to deal with this
	ad = strchr(dag, ' ') + 1;
	p = strchr(ad, ' ');
	*p = 0;
	hid = p + 1;
	p = strchr(hid, ' ');
	*p = 0;


	// send the file request
	sprintf(cmd, "get %s",  fin);
	sendCmd(sock, cmd);

	// get back number of chunks in the file
	getReply(sock, reply, sizeof(reply));

	int count = atoi(&reply[4]);

	if ((chunkSock = Xsocket(XSOCK_CHUNK)) < 0)
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

		getReply(sock, reply, sizeof(reply));
		offset += NUM_CHUNKS;

		if (getFileData(chunkSock, f, &reply[4]) < 0) {
			status= -1;
			break;
		}
	}
	
	fclose(f);

	if (status < 0) {
		unlink(fin);
	}

	say("shutting down\n");
	sendCmd(sock, "done");
	Xclose(sock);
	Xclose(chunkSock);
	return status;
}
