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
** - add ls cmd
** - add ability to handle put as well as get
** - return url for multiple dags instead of 1 at a time?
*/

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include "xcache.h"
#include "Xsocket.h"
#include "dagaddr.hpp"
#include "Xkeys.h"

#define VERSION "v2.0"
#define TITLE "XIA Basic FTP Server"
#define NAME "basicftp.xia"
#define BUFSIZE 1000

#define MB(__mb) (__mb * 1024 * 1024)
#define MAXCHUNKSIZE MB(10)

int verbose = 0;
unsigned ttl = 0;
bool setdir = false;
char name[256];
char rootdir[256];
unsigned chunksize = MB(1);
XcacheHandle xcache;

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-v] [-n name] [-c chunk_size] [-r root_dir] [-t chunk_lifetime]\n", name);
	printf("where:\n");
	printf(" -v : verbose mode\n");
	printf(" -n : specify service name (default = %s)\n", NAME);
	printf(" -c : maximum chunk size (default = 1M)\n");
	printf(" -r : specify the base directory we are running from\n");
	printf(" -t : specify the ttl for chunks in seconds\n");
	printf("\n");
	exit(0);
}

void say(const char *fmt, ...);

/*
** configure the app
*/
void getConfig(int argc, char** argv)
{
	int c;

	strcpy(name, NAME);
	(void*)getcwd(rootdir, sizeof(rootdir));

	opterr = 0;

	while ((c = getopt(argc, argv, "c:hn:r:t:v")) != -1) {
		switch (c) {
			case 'c':
				if ((chunksize = atoi(optarg)) != 0) {
					chunksize *= 1024;
				}
				break;

			case 'v':
				verbose = 1;
				break;
			case 'n':
				strcpy(name, optarg);
				break;
			case 'r':
				strcpy(rootdir, optarg);
				setdir = true;
				break;
			case 't':
				ttl = atoi(optarg);
				break;
			case '?':
			case 'h':
			default:
				// Help Me!
				help(basename(argv[0]));
				break;
		}
	}
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

void *recvCmd(void *socketid)
{
	int n, count = 0;
	sockaddr_x *addrs = NULL;
	char command[BUFSIZE];
	char reply[BUFSIZE];
	int sock = *((int*)socketid);
	char *fname;


	while (1) {
		say("waiting for command\n");

		memset(command, '\0', sizeof(command));
		memset(reply, '\0', sizeof(reply));

		if ((n = Xrecv(sock, command, 1024, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		} else if (n == 0) {
			warn("Peer closed the connection\n");
 			break;
		}

		if (strncmp(command, "get", 3) == 0) {
			fname = &command[4];
			say("client requested file %s\n", fname);

			//Chunking is done by the XputFile which itself uses XputChunk, and fills out the info
			if ((count = XputFile(&xcache, fname, chunksize, &addrs)) < 0) {
				warn("unable to serve the file: %s\n", fname);
				sprintf(reply, "FAIL: File (%s) not found", fname);
			} else {
				sprintf(reply, "OK: %d", count);
			}
			say("%s\n", reply);

			// tell the receiver how many chunks it should expect
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		} else if (strncmp(command, "block", 5) == 0) {
			char *chk = &command[6];
			int i = atoi(chk);
			char url[256];

			Graph g(&addrs[i]);
			g.fill_sockaddr(&addrs[i]);

			if (snprintf(reply, BUFSIZE, "OK: %s", g.http_url_string().c_str()) > BUFSIZE) {
				// this shouldn't happen
				strcpy(reply, "FAIL:buffer too small.");
				say("reply: %s\n", reply);
				if (Xsend(sock, reply, strlen(reply), 0) < 0) {
					warn("unable to send reply to client\n");
					break;
				}
				continue;

			} else {
				strcat(reply, " ");
				strcat(reply, url);
			}
			say("reply:%s\n", reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}

		} else if (strncmp(command, "done", 4) == 0) {
			say("done sending file: %s\n", fname);
			if (addrs) {
				free(addrs);
			}
			count = 0;
		}
	}

	Xclose(sock);
	pthread_exit(NULL);
}


//Just registering the service and opening the necessary sockets
int registerReceiver()
{
	int sock;
	char ttls[20];
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];

	sprintf(ttls, "%u(s)", ttl);

	say ("\n%s (%s)\n", TITLE, VERSION);
	say("Service Name: %s\n", name);
	say("Root Directory: %s\n", rootdir);
	say("Max Chunk Size: %u\n", chunksize);
	say("Time-to-live: %s\n\n", (ttl == 0 ? "forever" : ttls));

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	struct addrinfo *ai;

	if(XmakeNewSID(sid_string, sizeof(sid_string))) {
		die(-1, "Unable to create a temporary SID");
	}

	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) != 0)
		die(-1, "getaddrinfo failure!\n");

	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;
	if (XregisterName(name, dag) < 0 )
		die(-1, "error registering name: %s\n", NAME);

	if (Xbind(sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	Xlisten(sock, 5);

	Graph g(dag);
	say("listening on dag: %s\n", g.dag_string().c_str());

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

int main(int argc, char **argv)
{
	getConfig(argc, argv);

	

	if (XcacheHandleInit(&xcache) < 0) {
		die(-1, "Unable to initialze the cache subsystem\n");
	}
	sockaddr_x *info = NULL;
	XputFile(&xcache, "1", chunksize, &info);
	XputFile(&xcache, "2", chunksize, &info);
	XputFile(&xcache, "3", chunksize, &info);
	XputFile(&xcache, "4", chunksize, &info);
	XputFile(&xcache, "5", chunksize, &info);
	XcacheHandleSetTtl(&xcache, ttl);

	int sock = registerReceiver();
	// set the base directory. This affects all paths for the app,
	// so do it after we've initialized everything otherwise the paths
	// to /tmp and the keys directory will be incorrect
	if (setdir) {
		if (chroot(rootdir) < 0) {
			die(-1, "\nUnable to chroot to %s: %s\n", rootdir, strerror(errno));
		}
		chdir("/");
	}

	blockingListener((void *)&sock);
	XcacheHandleDestroy(&xcache);


	return 0;
}
