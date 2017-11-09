#include "push-demo-client.h"

#define BUFSIZE 1024

PushDemoClient::PushDemoClient()
{
	_sid_strlen = 1+ strlen("SID:") + XIA_SHA_DIGEST_STR_LEN;
	_sid_string = (char *)malloc(_sid_strlen);
	memset(_sid_string, 0, _sid_strlen);
	if(_sid_string == NULL) {
		throw "Unable to allocate memory for SID string";
	}
	std::cout << "Starting PushDemoClient" << std::endl;

	if((_sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		throw "Unable to create socket for client";
	}

	if(XmakeNewSID(_sid_string, _sid_strlen)) {
		throw "Unable to create a new SID";
	}

	struct addrinfo hints;
	struct addrinfo *ai;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_XIA;
	if(Xgetaddrinfo(NULL, _sid_string, &hints, &ai) != 0) {
		throw "getaddrinfo failed";
	}
	sockaddr_x *sa = (sockaddr_x *)ai->ai_addr;
	Graph g(sa);
	std::cout << "Client addr: " << g.dag_string() << std::endl;

	if(Xbind(_sockfd, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0) {
		throw "Unable to bind to client address";
	}

	Xlisten(_sockfd, 1);

	// Get a handle to Xcache
	/*
	if(XcacheHandleInit(&_xcache)) {
		throw "Unable to talk to xcache";
	}
	*/
}

PushDemoClient::~PushDemoClient()
{
	// Cleanup state before exiting
	if (_sockfd >= 0) {
		Xclose(_sockfd);
	}
	if(_sid_string) {
		free(_sid_string);
	}
}

int PushDemoClient::fetch()
{
	while (1) {
		int n = 0;
		int sock;
		sockaddr_x sa;
		socklen_t addrlen;
		char buf[BUFSIZE];

		std::cout << "Socket " << _sockfd << " waiting" << std::endl;

		sock = Xaccept(_sockfd, (sockaddr *)&sa, &addrlen);
		if(sock < 0) {
			std::cout << "Xaccept failed. Continuing..." << std::endl;
			continue;
		}
		Graph g(&sa);
		std::cout << "Socket " << sock << " connected to " <<
			g.dag_string() << std::endl;

		int count = 0;
		while((count=Xrecv(sock, buf, sizeof(buf), 0)) != 0) {
			std::cout << sock << " Got " << count << " bytes" << std::endl;
			n += count;
		}
		Xclose(sock);
		std::cout << "Got " << n << " total bytes" << std::endl;
	}
	return 0;
}

int main()
{
	PushDemoClient *client = new PushDemoClient();
	int retval = client->fetch();
	delete client;
	return retval;
}
