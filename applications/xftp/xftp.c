/* ts=4 */
/*
** Copyright 2011/2016 Carnegie Mellon University
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

/*
** TODO:
** - is the notification thread doing anything?
** - set verbose flag from cmdline
** - add ability to put as well as get files
** - add scp-like cmdline ability
*/

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "Xsocket.h"
#include "xcache.h"
#include "dagaddr.h"

#define VERSION "v2.0"
#define TITLE "XIA Basic FTP client"
#define NAME "basicftp.xia"
#define MAX_CHUNKSIZE (10 * 1024 * 1024)	// set upper limit since we don't know how big chunks will be

// global configuration options
int verbose = 1;

XcacheHandle h;

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

int getResponse(int sock, char *reply, int sz)
{
	int n;

	if ((n = Xrecv(sock, reply, sz, 0))  < 0) {
		Xclose(sock);
		die(-1, "Unable to communicate with the server\n");
	}

	if (strncmp(reply, "OK:", 3) != 0) {
		warn( "%s\n", reply);
		return -1;
	}

	reply[n] = 0;

	return n;
}


int retrieveChunk(FILE *fd, char *url)
{
	char *saveptr, *token;
	char *buf = (char*)malloc(MAX_CHUNKSIZE);

	say("Received url = %s\n", url);
	token = strtok_r(url, " ", &saveptr);
	while (token) {
		int ret;
		sockaddr_x addr;

		say("Fetching URL %s\n", token);
		url_to_dag(&addr, token, strlen(token));

//		Graph g(&addr);
//		printf("------------------------\n");
//		g.print_graph();
//		printf("------------------------\n");

		if ((ret = XfetchChunk(&h, buf, MAX_CHUNKSIZE, XCF_BLOCK, &addr, sizeof(addr))) < 0) {
		 	die(-1, "XfetchChunk Failed\n");
		}

		say("Got Chunk\n");
		fwrite(buf, 1, ret, fd);
		token = strtok_r(NULL, " ", &saveptr);
	}

	free(buf);
	return 0;
}

int getFile(int sock, const char *fin, const char *fout)
{
	int offset;
	char cmd[5120];
	char reply[5120];
	int status = 0;

	// send the file request
	sprintf(cmd, "get %s",  fin);
	sendCmd(sock, cmd);

	// get back number of chunks in the file
	if (getResponse(sock, reply, sizeof(reply)) < 1){
		warn("could not get chunk count. Aborting. \n");
		return -1;
	}

	int count = atoi(&reply[4]);
	printf("Count = %d\n", count);

	FILE *f = fopen(fout, "w");

	offset = 0;
	while (offset < count) {
		sprintf(cmd, "block %d", offset);
		sendCmd(sock, cmd);

		if (getResponse(sock, reply, sizeof(reply)) < 1) {
			warn("could not get chunk count. Aborting. \n");
			return -1;
		}
		offset++;
		say("reply = %s\n", &reply[4]);
		if (retrieveChunk(f, &reply[4]) < 0) {
			warn("error retreiving: %s\n", &reply[4]);
			status= -1;
			break;
		}
	}

	fclose(f);

	if (status < 0) {
		unlink(fin);
	}

	say("Received file %s\n", fout);
	sendCmd(sock, "done");
	return status;
}

void xcache_chunk_arrived(XcacheHandle *h, int /*event*/, sockaddr_x *addr, socklen_t addrlen)
{
	char buf[512];

	printf("Received Chunk Arrived Event\n");

	printf("XreadChunk returned %d\n", XreadChunk(h, addr, addrlen, buf, sizeof(buf), 0));
}

int initializeClient(const char *name)
{
	int sock;
	sockaddr_x dag;
	socklen_t daglen;

	if (XcacheHandleInit(&h) < 0) {
		printf("Xcache handle initialization failed.\n");
		exit(-1);
	}

	XregisterNotif(XCE_CHUNKARRIVED, xcache_chunk_arrived);
	XlaunchNotifThread(&h);

    // lookup the xia service
	daglen = sizeof(dag);
	if (XgetDAGbyName(name, &dag, &daglen) < 0)
		die(-1, "unable to locate: %s\n", name);

	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		die(-1, "Unable to connect to: %s\n%s\n", name, dag);
	}

	return sock;
}

void usage(){
	warn("usage: get <source file> <dest name>\n       quit\n");
}

int main(int argc, char **argv)
{
	const char *name;
	int sock = -1;
	char fin[512], fout[512];
	char cmd[512], reply[512];
	int params = -1;

	say ("\n%s (%s): started\n", TITLE, VERSION);

	if ( argc == 1) {
		say ("No service name passed, using default: %s\n", NAME);
		sock = initializeClient(NAME);
		usage();

	} else if (argc == 2) {
		name = argv[1];
		say ("Connecting to: %s\n", name);
		sock = initializeClient(name);
		usage();

	} else {
		die(-1, "xftp [service name]");
	}

	while (true) {
		say(">>");
		cmd[0] = fin[0] = fout[0] = 0;
		params = -1;

		if (fgets(cmd, sizeof(cmd) - 1, stdin) == NULL) {
			die(errno, "%s", strerror(errno));
		}

		if (strncasecmp(cmd, "get", 3) == 0){
			params = sscanf(cmd,"get %s %s", fin, fout);

			if (params != 2) {
				sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
				warn(reply);
				usage();
				continue;
			}

			getFile(sock, fin, fout);

		} else if (strncasecmp(cmd, "quit", 4) == 0) {
			XcacheHandleDestroy(&h);
			Xclose(sock);
			exit(0);
		}
	}
	return 1;
}
