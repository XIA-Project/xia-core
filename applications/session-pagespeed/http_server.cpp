#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <netdb.h>
#include <stdio.h>
#include <sys/stat.h>
#include "session.h"
#include "utils.hpp"

using namespace std;

#define DEBUG

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: INFO  %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: INFO  " fmt"\n", __FILE__, __LINE__, __VA_ARGS__) 
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif
#define ERROR(s) fprintf(stderr, "\033[0;31m%s:%d: ERROR  %s\n\033[0m", __FILE__, __LINE__, s)
#define ERRORF(fmt, ...) fprintf(stderr, "\033[0;31m%s:%d: ERROR  " fmt"\n\033[0m", __FILE__, __LINE__, __VA_ARGS__) 


#define MAXBUF 4096

















#define SERVER "XIA Session Webserver/1.0"
#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

string get_mime_type(const char *name)
{
	const char *ext = strrchr(name, '.');
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".gif") == 0) return "image/gif";
	if (strcmp(ext, ".png") == 0) return "image/png";
	if (strcmp(ext, ".css") == 0) return "text/css";
	if (strcmp(ext, ".au") == 0) return "audio/basic";
	if (strcmp(ext, ".wav") == 0) return "audio/wav";
	if (strcmp(ext, ".avi") == 0) return "video/x-msvideo";
	if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) return "video/mpeg";
	if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
	return NULL;
}

int send_header(int ctx, int status, const char *title, const char *extra, const char *mime, 
                  int length, time_t date) {
LOG("BEGIN send_header");
	string header;
	time_t now;
	char strbuf[4096];
	char timebuf[128];

	sprintf(strbuf, "%s %d %s\r\n", PROTOCOL, status, title);
	header += strbuf;

	sprintf(strbuf, "Server: %s\r\n", SERVER);
	header += strbuf;

	now = time(NULL);
	strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
	sprintf(strbuf, "Date: %s\r\n", timebuf);
	header += strbuf;

	if (extra) {
		sprintf(strbuf, "%s\r\n", extra);
		header += strbuf;
	}
	if (mime) {
		sprintf(strbuf, "Content-Type: %s\r\n", mime);
		header += strbuf;
	}
	if (length >= 0) {
		sprintf(strbuf, "Content-Length: %d\r\n", length);
		header += strbuf;
	}
	if (date != -1)
	{
		strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&date));
		sprintf(strbuf, "Last-Modified: %s\r\n", timebuf);
		header += strbuf;
	}

	sprintf(strbuf, "Connection: close\r\n");
	header += strbuf;

	sprintf(strbuf, "\r\n");
	header += strbuf;

LOGF("Attempting to send header:\n%s", header.c_str());
	int rc;
	if ( (rc = Ssend(ctx, header.c_str(), header.size())) < 0) {
		ERROR("Error sending header");
	}

	return rc;
}

int send_error(int ctx, int status, const char *title, const char *extra, const char *text)
{
	char strbuf[4096];

	if (send_header(ctx, status, title, extra, "text/html", -1, -1)) {
		ERROR("Error sending header");
		return -1;
	}

	string msg;
	sprintf(strbuf, "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\r\n", status, title);
	msg += strbuf;

	sprintf(strbuf, "<BODY><H4>%d %s</H4>\r\n", status, title);
	msg += strbuf;

	sprintf(strbuf, "%s\r\n", text);
	msg += strbuf;

	sprintf(strbuf, "</BODY></HTML>\r\n");
	msg += strbuf;

	int rc;
	if ( (rc = Ssend(ctx, msg.data(), msg.size())) < 0) {
		ERROR("Error sending error HTTP message");
	}

	return rc;
}

int send_file(int ctx, const char *path, struct stat *statbuf) {
LOG("BEGIN send_file");
	char data[4096];
	int n;

	FILE *file = fopen(path, "r");
	if (!file)
		send_error(ctx, 403, "Forbidden", NULL, "Access denied.");
	else
	{
		int length = S_ISREG(statbuf->st_mode) ? statbuf->st_size : -1;
		send_header(ctx, 200, "OK", NULL, get_mime_type(path).c_str(), length, statbuf->st_mtime);

		int bytesSent = 0;
//int total = 0;
//int pktcnt = 0;
		while ((n = fread(data, 1, sizeof(data), file)) > 0) {
			bytesSent = Ssend(ctx, data, n);
//total += bytesSent;
//pktcnt++;
			usleep(1000);
//LOGF("Sent %d bytes; n=%d; cumm total: %d, packet # %d", bytesSent, n, total, pktcnt);
		}
		fclose(file);

		Ssend(ctx, "\r\n\r\n", 4);
	}

	return 0;
}





int process(int ctx)
{
	char buf[MAXBUF];
	char *method;
	char *fullpath;
	const char *path;
	char *protocol;
	struct stat statbuf;

	/* Receive request */
	memset(&buf[0], 0, sizeof(buf));
	if (SrecvADU(ctx, buf, MAXBUF) < 0) {
	    printf("Error receiving data\n");
		return -1;
	}

	LOGF("Request:\n%s", buf);

	method = strtok(buf, " ");
	fullpath = strtok(NULL, " ");
	protocol = strtok(NULL, "\r");

	// TODO: make this better
	// get rid of the hostname in the path
	string pathStr = "./www/";
	string tempStr = string(fullpath);
	size_t found = tempStr.find("/");
	found = tempStr.find("/", found+1);
	found = tempStr.find("/", found+1);
	pathStr += tempStr.erase(0, found+1);
	path = pathStr.c_str();
	LOGF("Path: %s", path);

	if (!method || !path || !protocol) return -1;


	if (strcasecmp(method, "GET") != 0)
		send_error(ctx, 501, "Not supported", NULL, "Method is not supported.");
	else if (stat(path, &statbuf) < 0)
		send_error(ctx, 404, "Not Found", NULL, "File not found.");
	else if (S_ISDIR(statbuf.st_mode))
	{
		ERROR("Directories not supported");
	}
	else
		send_file(ctx, path, &statbuf);

	return 0;
}


int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

    int listen_ctx, accept_ctx;

    // Make "listen" context
	listen_ctx = SnewContext();
	if (listen_ctx < 0) {
		ERROR("Error creating new context");
		exit(-1);
	}

    // Bind to name "server"
	if ( Sbind(listen_ctx, "www.test.cmu.edu") < 0 ) {
		ERROR("Error binding");
		exit(-1);
	}

    while (1) {
    
		LOG("Listening...");
    	accept_ctx = SacceptConnReq(listen_ctx);
		if (accept_ctx < 0) {
			ERROR("Error accepting connection request");
			exit(-1);
		}
    	LOG("Accepted a new connection");
    	
    	pid_t pid = fork();
    
    	if (pid == 0) {  
    		// child
    		
    		while (1) { // TODO: exit?
				process(accept_ctx);
    		}
    	}
    }
    return 0;
}
