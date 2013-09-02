#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <netdb.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fstream>
#include <errno.h>
#include "session.h"
#include "utils.hpp"
#include "http.hpp"

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


int process_image_request(int ctx, string &http_header) {
	LOGF("Ctx %d    Got an image:\n%s", ctx, http_header.c_str());

	/* get image size */
	int img_size = contentLengthFromHTTP(http_header);
	LOGF("Ctx %d    Content-Length is %d", ctx, img_size);

	/* write the image to a file */
	char *tmpname = strdup("/tmp/tmpfileXXXXXX.jpg");
	if (mkstemps(tmpname, 4) < 0) {
		ERRORF("Ctx %d    Error making temp file: %s", ctx, strerror(errno));
		return -1;
	}
	LOGF("Ctx %d    Saving image to temp file: %s", ctx, tmpname);
	ofstream tempFile(tmpname);
    //ofstream tempFile("temp.jpeg", ios::out | ios::binary); 
	char buf[MAXBUF];
	int received = 0;
	int total = 0;
	while (total < img_size) {
		if ( (received = Srecv(ctx, buf, MAXBUF)) < 0 ) {
			ERROR("Error receiving");
			return -1;
		}
		tempFile.write(buf, received);

		total += received;
		//LOGF("Ctx %d    Received %d of %d bytes", ctx, total, img_size);
	}
	tempFile.close();


	/* scale down temp.jpeg */
	string cmd = "/usr/bin/mogrify -resize 320x " + string(tmpname);
	if (system(cmd.c_str()) < 0) {
		ERRORF("Ctx %d    Error scaling image: %s", ctx, strerror(errno));
	}


	/* send temp.jpeg */
	LOGF("Ctx %d    Sending temp file: %s", ctx, tmpname);
	struct stat statbuf;
	stat(tmpname, &statbuf);
	send_file(ctx, tmpname, &statbuf);


	/* remove temp file */
	if (remove(tmpname) < 0 ) {
		ERRORF("Ctx %d    Error removing temp file %s: %s", ctx, tmpname, strerror(errno));
	}

LOGF("Ctx %d    Finished processing image", ctx);
	return 0;
}


int process(int ctx)
{
	LOGF("Processing new connection on context %d", ctx);
	char buf[MAXBUF];
	int bytesReceived = 0;

	/* Receive first n bytes */
	memset(&buf[0], 0, sizeof(buf));
	if ( (bytesReceived = SrecvADU(ctx, buf, MAXBUF)) < 0) {
	    ERRORF("Ctx %d    Error receiving data", ctx);
		return -1;
	}

	string httpHeader(buf, bytesReceived);
	LOGF("Ctx %d    Received header:\n%s", ctx, httpHeader.c_str());

	// check if this is the beginning of an HTTP header for an image
	bool image = false;
	if (httpHeader.find("200 OK") != httpHeader.npos && httpHeader.find("image/jpeg") != httpHeader.npos) {
		image = true;
	}

	/* If this is the start of an image, process_image_request will keep receiving
	   until it gets the whole image, scales it, sends it on, and returns. Otherwise,
	   we just forward on the data we got and return. The next call to process might
	   be data or another HTTP header. */
	if (image) {
		return process_image_request(ctx, httpHeader);
	} else {
		LOGF("Ctx %d    Forwarding non-image HTTP message", ctx);

		/* forward the response HTTP header */
		if (Ssend(ctx, buf, bytesReceived) < 0) {
			ERRORF("Ctx %d    Error sending response header", ctx);
			return -1;
		}

		/* receive & forward the # bytes specified by content length */
		int contentLength = contentLengthFromHTTP(httpHeader);
		int total = 0;
		while (total < contentLength) {
			if ( (bytesReceived = SrecvADU(ctx, buf, MAXBUF)) < 0) {
				ERRORF("Ctx %d    Error receiving", ctx);
				return -1;
			}

			if (Ssend(ctx, buf, bytesReceived) < 0) {
				ERRORF("Ctx %d    Error sending", ctx);
				return -1;
			}

			total += bytesReceived;
		} 
	}


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

    // Bind to name
	if ( Sbind(listen_ctx, "pagespeed.cmu.edu") < 0 ) {
		ERROR("Error binding");
		exit(-1);
	}

    while (1) {
    
		LOGF("Listening for a new connection on context %d", listen_ctx);
    	accept_ctx = SacceptConnReq(listen_ctx);
		if (accept_ctx < 0) {
			ERROR("Error accepting connection request");
			exit(-1);
		}
    	LOG("Accepted a new connection");
    	
    	pid_t pid = fork();
    
    	if (pid == 0) {  
    		// child
    		
    		//while (1) { // TODO: exit?
				process(accept_ctx);
    		//}
			break;
    	}
    }
    return 0;
}
