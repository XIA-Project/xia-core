#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <netdb.h>
#include <stdio.h>
#include <sys/stat.h>
#include <vector>
#include <string.h>
#include "session.h"
#include "utils.hpp"
#include "http.hpp"

using namespace std;

#define SERVER "XIA Session Webserver/1.0"
#define PROTOCOL "HTTP/1.0"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

string get_mime_type(const char *name)
{
	const char *ext = strrchr(name, '.');
	if (!ext) return NULL;
	if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html";
	if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if (strcmp(ext, ".tif") == 0 || strcmp(ext, ".tiff") == 0) return "image/tiff";
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
LOGF("Ctx %d    BEGIN send_header", ctx);
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
LOGF("Ctx %d    BEGIN send_file", ctx);
	char data[4096];
	int n;

	FILE *file = fopen(path, "r");
	if (!file) {
		send_error(ctx, 403, "Forbidden", NULL, "Access denied.");
	} else {
		int length = S_ISREG(statbuf->st_mode) ? statbuf->st_size : -1;
		send_header(ctx, 200, "OK", NULL, get_mime_type(path).c_str(), length, statbuf->st_mtime);

		while ((n = fread(data, 1, sizeof(data), file)) > 0) {
			if (Ssend(ctx, data, n) < 0) {
				ERROR("Error sending");
			}
			usleep(20 * 1000);
		}
		fclose(file);

		Ssend(ctx, "\r\n\r\n", 4);
	}

	return 0;
}

int contentLengthFromHTTP(string &httpHeader) {
	vector<string> elements = split(httpHeader, '\n');
	vector<string>::iterator it;
	int img_size = -1;
	for (it = elements.begin(); it != elements.end(); ++it) {
		if ((*it).find("Content-Length:") != (*it).npos) {
			img_size = atoi(trim(split((*it), ':')[1]).c_str());
			break;
		}
	}

	return img_size;
}
		
string hostNameFromHTTP(string &httpHeader) {
	string host;
	vector<string> elements = split(httpHeader, '\n');
	vector<string>::iterator it;
	for (it = elements.begin(); it != elements.end(); ++it) {
		if ((*it).find("Host:") != (*it).npos) {
			host = trim(split((*it), ' ')[1]);
			break;
		}
	}

	return host;
}
