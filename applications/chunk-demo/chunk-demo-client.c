#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
#include "xcache.h"
#include "dagaddr.hpp"
#include "dagaddr.h"
#include <assert.h>
#include "chunk-demo.h"

int verbose = 1;
char s_ad[MAX_XID_SIZE];
char s_hid[MAX_XID_SIZE];

char my_ad[MAX_XID_SIZE];
char my_hid[MAX_XID_SIZE];

XcacheHandle h;

int getFile(int sock, char *p_ad, char* p_hid, const char *fin, const char *fout);

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

int get_url(int sock, char *reply, int sz)
{
	int n = -1;

	if ((n = Xrecv(sock, reply, sz, 0))  < 0) {
		Xclose(sock);
		die(-1, "Unable to communicate with the server\n");
	}

	reply[n] = 0;

	return n;
}

int make_and_getchunk(int sock)
{
	int offset;
	char cmd[5120];
	char url[5120];
	int status = 0;
	sockaddr_x addr;
	char buf[1024 * 1024];
	int ret;

	// send the file request
	sprintf(cmd, "make");
	sendCmd(sock, cmd);

	// get back number of chunks in the file
	if (get_url(sock, url, sizeof(url)) < 1){
		warn("could not get chunk count. Aborting. \n");
		return -1;
	}

	printf("Got URL %s\n", url);

	url_to_dag(&addr, url, strlen(url));
	if ((ret = XfetchChunk(&h, buf, 1024 * 1024, 0, &addr,
			       sizeof(addr))) < 0) {
		die(-1, "XfetchChunk Failed\n");
	}
}

void xcache_chunk_arrived(XcacheHandle *h, int /*event*/, sockaddr_x *addr, socklen_t addrlen)
{
	char buf[CHUNKSIZE];
	int i;

	printf("Received Chunk Arrived Event\n");

	printf("XreadChunk returned %d\nData: \n", XreadChunk(h, addr, addrlen, buf, sizeof(buf), 0));
	for (i = 0; i < CHUNKSIZE; i++) {
		if (i % 16 == 0) {
			printf("%.8x ", i);
		}

		printf("%02x", (unsigned char)buf[i]);
		if (i % 2 == 1)
			printf(" ");

		if ((i + 1) % 16 == 0)
			printf("\n");
	}
}

int initializeClient(const char *name)
{
	int sock, rc;
	sockaddr_x dag;
	socklen_t daglen;
	char sdag[1024];
	char IP[MAX_XID_SIZE];

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


	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");

	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	rc = XreadLocalHostAddr(sock, my_ad, MAX_XID_SIZE, my_hid, MAX_XID_SIZE, IP, MAX_XID_SIZE);

	if (rc < 0) {
		Xclose(sock);
		die(-1, "Unable to read local address.\n");
	} else{
		warn("My AD: %s, My HID: %s\n", my_ad, my_hid);
	}

	// save the AD and HID for later. This seems hacky
	// we need to find a better way to deal with this
	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
//   	say("sdag = %s\n",sdag);
	char *ads = strstr(sdag,"AD:");
	char *hids = strstr(sdag,"HID:");
// 	i = sscanf(ads,"%s",s_ad );
// 	i = sscanf(hids,"%s", s_hid);

	if (sscanf(ads,"%s",s_ad ) < 1 || strncmp(s_ad,"AD:", 3) !=0){
		die(-1, "Unable to extract AD.");
	}

	if (sscanf(hids,"%s", s_hid) < 1 || strncmp(s_hid,"HID:", 4) !=0 ){
		die(-1, "Unable to extract AD.");
	}

	warn("Service AD: %s, Service HID: %s\n", s_ad, s_hid);

	return sock;
}

void usage(){
	say("usage: get|put <source file> <dest name>\n");
}

bool file_exists(const char * filename)
{
    if (FILE * file = fopen(filename, "r")){
		fclose(file);
		return true;
	}
    return false;
}

int main(int argc, char **argv)
{

	const char *name;
	int sock = -1;
	char fin[512], fout[512];
	char cmd[512], reply[512];
	int params = -1;

	say ("\n%s (%s): started\n", TITLE, VERSION);

	if (argc == 1) {
		say("No service name passed, using default: %s\n", NAME);
		sock = initializeClient(NAME);
		usage();
	} else if (argc == 2) {
		name = argv[1];
		say("Connecting to: %s\n", name);
		sock = initializeClient(name);

	} else {
		die(-1, "chunk-demo-client [SID]");
	}


	while (1) {
		say(">>");
		cmd[0] = '\n';
		fin[0] = '\n';
		fout[0] = '\n';
		params = -1;

		fgets(cmd, 511, stdin);

		if (strncmp(cmd, "make", 4) == 0){
			make_and_getchunk(sock);
		}
	}
	return 1;
}
