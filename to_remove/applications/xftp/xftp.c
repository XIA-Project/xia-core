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
** - add non-blocking option for better test coverage
** - add ability to put as well as get files
** - add scp-like cmdline ability
*/

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include "Xsocket.h"
#include "xcache.h"
#include "dagaddr.hpp"

#define VERSION "v2.0"
#define TITLE "XIA Basic FTP client"
#define NAME "basicftp.xia"
#define MAX_CHUNKSIZE (10 * 1024 * 1024)	// set upper limit since we don't know how big chunks will be

// global configuration options
int verbose = 0;
char name[256];
int global_flags = 0;
double totalElapseTime = 0.0;
XcacheHandle h;
int chunks_arrived = 0;
int chunks_requested = 0;
bool nonblocking = false;
std::vector<Graph> chunks;
pthread_mutex_t chunkstatus = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t chunks_ready = PTHREAD_COND_INITIALIZER;

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-v] [-n name]\n", name);
	printf("where:\n");
	printf(" -c : cache content locally after fetching\n");
	printf(" -b : fetch content without blocking (implies -c)\n");
	printf(" -v : verbose mode\n");
	printf(" -n : remote service name (default = %s)\n", NAME);
	printf("\n");
	exit(0);
}

/*
** configure the app
*/
void getConfig(int argc, char** argv)
{
	int c;

	strcpy(name, NAME);

	opterr = 0;

	while ((c = getopt(argc, argv, "chn:vb")) != -1) {
		switch (c) {
			case 'v':
				verbose = 1;
				break;
			case 'n':
				strcpy(name, optarg);
				break;
			case 'c':
				global_flags = XCF_CACHE;
				break;
			case 'b':
				nonblocking = true;
				global_flags = XCF_CACHE;
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

int sendCmd(int sock, const char *cmd)
{
	int n;
	say("Sending Command: %s \n", cmd);
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
	void *buf;
	double elapsedTime;
    struct timeval t1, t2;

	token = strtok_r(url, " ", &saveptr);
	while (token) {
		int ret;
		sockaddr_x addr;

		say("Fetching URL %s\n", token);

		Graph g(token);
		g.fill_sockaddr(&addr);

		if (nonblocking) {
			ret = XfetchChunk(&h, &buf, global_flags, &addr, sizeof(addr));
			if (ret < 0) {
				die(-1, "XfetchChunk Failed with %d\n", ret);
			}
			pthread_mutex_lock(&chunkstatus);
			chunks_requested++;
			pthread_mutex_unlock(&chunkstatus);
			chunks.push_back(g);
			token = strtok_r(NULL, " ", &saveptr);
			continue;
		}

        gettimeofday(&t1, NULL);
		if ((ret = XfetchChunk(&h, &buf, XCF_BLOCK | global_flags, &addr, sizeof(addr))) < 0) {
		 	die(-1, "XfetchChunk Failed\n");
		}
		gettimeofday(&t2, NULL);

		printf("------------------------\n");
		g.print_graph();
		printf("------------------------\n");


		elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
        elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms

        say("current elapsedTime: %f\n", elapsedTime);

        totalElapseTime += elapsedTime; 		// in ms

		say("Got Chunk\n");
		fwrite((char *)buf, 1, ret, fd);
		token = strtok_r(NULL, " ", &saveptr);
		free(buf);
	}

	return 0;
}

int getFile(int sock, const char *fin, const char *fout)
{
	int offset;
	char cmd[5120];
	char reply[5120];
	int status = 0;

	if (nonblocking) {
		chunks_arrived = 0;
		chunks_requested = 0;
	}
	// send the file request
	sprintf(cmd, "get %s",  fin);
	sendCmd(sock, cmd);

	// get back number of chunks in the file
	if (getResponse(sock, reply, sizeof(reply)) < 1){
		warn("could not get chunk count. Aborting. \n");
		return -1;
	}

	int count = atoi(&reply[4]);
	say("Chunk Count = %d\n", count);

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
		if (retrieveChunk(f, &reply[4]) < 0) {
			warn("error retreiving: %s\n", &reply[4]);
			status= -1;
			break;
		}
	}
	// When all chunks are ready, fetch them
	if (nonblocking) {
		pthread_mutex_lock(&chunkstatus);
		// Wait until chunks_arrived == chunks_requested
		while(chunks_arrived != chunks_requested) {
			// FIXME change to pthread_cond_timedwait so we can time out
			pthread_cond_wait(&chunks_ready, &chunkstatus);
		}
		pthread_mutex_unlock(&chunkstatus);

		// Then fetch them all and write to file
		nonblocking = false; // Will block; chunk is in cache now
		std::vector<Graph>::iterator it;
		for (it = chunks.begin(); it !=chunks.end(); it++) {
			Graph g(*it);
			if (retrieveChunk(f, (char *)g.http_url_string().c_str()) < 0) {
				warn("error retreiving: %s\n", g.http_url_string().c_str());
				status= -1;
				break;
			}
		}
		chunks_arrived = 0;
		chunks_requested = 0;
		chunks.clear();
	}

	fclose(f);

	if (status < 0) {
		unlink(fin);
	}

	say("Received file %s\n", fout);
	say("Total elapsedTime: %f\n", totalElapseTime);
	sendCmd(sock, "done");
	return status;
}

// FIXME: does this ever get called???
void xcache_chunk_arrived(XcacheHandle *h, int event,
		void *data, size_t datalen)
{
	//int rc;
	//void *buf;
	if(event != XCE_CHUNKARRIVED) {
		printf("Received unexpected notification\n");
		return;
	}
	sockaddr_x *addr = (sockaddr_x *)data;
	socklen_t addrlen = (socklen_t)datalen;

	printf("Received Chunk Arrived Event\n");
	Graph g(addr);
	printf("From %s\n", g.dag_string().c_str());
	//chunks.push_back(Graph(addr));

	pthread_mutex_lock(&chunkstatus);
	chunks_arrived++;
	printf("%d chunks have arrived so far\n", chunks_arrived);
	if (chunks_arrived == chunks_requested) {
		printf("Chunks arrived: %d, requested %d\n", chunks_arrived,
				chunks_requested);
		pthread_cond_broadcast(&chunks_ready);
	}
	pthread_mutex_unlock(&chunkstatus);

	/*
	rc = XfetchChunk(h, &buf, XCF_BLOCK | global_flags, addr, addrlen);

	printf("XfetchChunk returned %d\n", rc);
	free(buf);
	*/
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
	warn("usage: get <source file> <dest name>\n       quit\n\n");
}

int main(int argc, char **argv)
{
	int sock = -1;
	char fin[512], fout[512];
	char cmd[512], reply[512];
	int params = -1;

	getConfig(argc, argv);

	say("\n%s (%s)\n", TITLE, VERSION);
	say("connecting to %s\n\n", name);

	sock = initializeClient(name);
	usage();

	while (true) {
		printf(">>");
		cmd[0] = fin[0] = fout[0] = 0;
		params = -1;

		if (fgets(cmd, sizeof(cmd) - 1, stdin) == NULL) {
			die(errno, "%s", strerror(errno));
		}

		if (strncasecmp(cmd, "get", 3) == 0){
			cmd[strlen(cmd) - 1] = 0;
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
