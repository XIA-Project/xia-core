/* ts=4 */
/*
** Copyright 2012, Carnegie Mellon University
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
** notes: (may not be up to date)
**
** Runs for 1.5 min video, may also work with lower resolution video for bit longer
**
** seems to have some issue currently ... for multiple clients
**  so first trying to do iterative with hacky way
*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>
#include <vector>
#include <sstream>
#include <iostream>
#ifdef __APPLE__
#include <libgen.h>
#endif
#include "Xsocket.h"
#include "Xkeys.h"

#define VERSION "v1.0"
#define TITLE "XIA Video Server"

#define CHUNKSIZE (1024)
#define SNAME "www_s.video.com.xia"

using namespace std;

// global configuration options
int verbose = 1;
string videoname;

int *acceptSock;

vector<string> CIDlist;

/*
** display cmd line options and exit
*/
void help(const char *name)
{
	printf("\n%s (%s)\n", TITLE, VERSION);
	printf("usage: %s [-q] file\n", name);
	printf("where:\n");
	printf(" -q : quiet mode\n");
	printf(" file is the video file to serve up\n");
	printf("\n");
	exit(0);
}

/*
** configure the app
*/
void getConfig(int argc, char** argv)
{
	int c;
	opterr = 0;

	while ((c = getopt(argc, argv, "q")) != -1) {
		switch (c) {
			case 'q':
				// turn off info messages
				verbose = 0;
				break;
			case '?':
			case 'h':
			default:
				// Help Me!
				help(basename(argv[0]));
		}
	}

	if (argc - optind != 1)
		help(basename(argv[0]));

	videoname = argv[optind];
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

/*
** upload the video file as content chunks
*/
int uploadContent(const char *fname)
{
	int count;

	say("loading video file: %s\n", fname);

	ChunkContext *ctx = XallocCacheSlice(POLICY_DEFAULT, 0, 20000000);
	if (ctx == NULL)
		die(-2, "Unable to initilize the chunking system\n");

	ChunkInfo *info;
	if ((count = XputFile(ctx, fname, CHUNKSIZE, &info)) < 0)
		die(-3, "unable to process the video file\n");

	say("put %d chunks\n", count);

   	for (int i = 0; i < count; i++) {
		string CID = "CID:";
		CID += info[i].cid;
		CIDlist.push_back(CID);
   	}

	XfreeChunkInfo(info);

	// close the connection to the cache slice, but becase it is set to retain,
	// the content will stay available in the cache
	XfreeCacheSlice(ctx);
	return 0;
}

/*
** handle the request from the client and return the requested data
*/
void *processRequest (void *socketid)
{
	int n;
	char SIDReq[1024];
	int *sock = (int*)socketid;
	int acceptSock = *sock;


	memset(SIDReq, 0, sizeof(SIDReq));

	//Receive packet

	if ((n = Xrecv(acceptSock, SIDReq, sizeof(SIDReq), 0)) > 0) {

		string SIDReqStr(SIDReq);
		// cout << "Got request: " << SIDReqStr << "\n";
		// if the request is about number of chunks return number of chunks
		// since this is first time, you would return along with header
		int found = SIDReqStr.find("numchunks");

		if(found != -1){
			// cout << " Request asks for number of chunks \n";
			stringstream yy;
			yy << CIDlist.size();
			string cidlistlen = yy.str();
			Xsend(acceptSock,(void *) cidlistlen.c_str(), cidlistlen.length(), 0);
		} else {
			// the request would have two parameters
			// start-offset:end-offset
			int findpos = SIDReqStr.find(":");
			// split around this position
			string str = SIDReqStr.substr(0,findpos);
			int start_offset = atoi(str.c_str());
			str = SIDReqStr.substr(findpos + 1);
			int end_offset = atoi(str.c_str());

			// construct the string from CIDlist
			// return the list of CIDs
			string requestedCIDlist = "";
			// not including end_offset
			for(int i = start_offset; i < end_offset; i++){
				requestedCIDlist += CIDlist[i] + " ";
			}
			Xsend(acceptSock, (void *)requestedCIDlist.c_str(), requestedCIDlist.length(), 0);
			cout << "sending " << requestedCIDlist << "\n";
		}
	}
	Xclose(acceptSock);
	pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
	sockaddr_x *dag;
	int sock;
	pthread_t client;
	char sid_string[256];

	getConfig(argc, argv);

	// put the video file into the content cache
	if (uploadContent(videoname.c_str()) != 0)
		die(-1, "Unable to upload the video %s\n", videoname.c_str());

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		 die(-1, "Unable to create the listening socket\n");

	// Generate an SID to use
	if(XmakeNewSID(sid_string, sizeof(sid_string)))
		die(-1, "Unable to create a temporary SID");

	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) < 0)
		 die(-1, "Unable to create the local dag\n");
	dag = (sockaddr_x*)ai->ai_addr;

	// register this service name to the name server
    if (XregisterName(SNAME, dag) < 0)
		perror("name register");

	if(Xbind(sock, (struct sockaddr*)dag, sizeof(sockaddr_x)) < 0)
		 die(-1, "Unable to bind to the dag: %s\n", dag);

	Xlisten(sock, 5);
	
	// we're done with this
	Xfreeaddrinfo(ai);


   	while (1) {
		say("\nListening...\n");

		acceptSock = (int *)calloc(1, sizeof(int));

		if ((*acceptSock = Xaccept(sock, NULL, NULL)) < 0) {
			free(acceptSock);
			die(-1, "accept failed\n");
        }
		say("We have a new connection\n");

		// handle the connection in a new thread
		if (pthread_create(&client, NULL, processRequest, acceptSock) != 0) {
			free(acceptSock);
        }
	}

	Xclose(sock); // we should never reach here!

	return 0;
}

