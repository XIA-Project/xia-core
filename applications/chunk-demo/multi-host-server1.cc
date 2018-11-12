#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"

#include <iostream>
#include <thread>
#include <chrono>

#define NAME "multi-host-server1.xia"

int main()
{
	int sock;

	if((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		std::cout << "Error creating socket to communicate" << std::endl;
		return -1;
	}

	size_t sid_strlen = sizeof("SID:") + XIA_SHA_DIGEST_STR_LEN;
	char sid_string[sid_strlen];

	if(XmakeNewSID(sid_string, sid_strlen)) {
		std::cout << "Error creating SID for this service" << std::endl;
		return -1;
	}

	std::string sid;
	sid.assign(sid_string, strlen(sid_string));
	std::cout << "Using: " << sid << std::endl;

	struct addrinfo *ai;
	if(Xgetaddrinfo(NULL, sid.c_str(), NULL, &ai) != 0) {
		std::cout << "ERROR getting address for this service" << std::endl;
		return -1;
	}

	sockaddr_x *dag = (sockaddr_x *)ai->ai_addr;

	if(Xbind(sock, (struct sockaddr *)dag, sizeof(sockaddr_x)) < 0) {
		Graph addr((sockaddr_x *) ai->ai_addr);
		std::cout << "ERROR binding to " << addr.dag_string() << std::endl;
		return -1;
	}

	if(XregisterName(NAME, dag) < 0) {
		std::cout << "ERROR sending our addr to nameserver" << std::endl;
		return -1;
	}

	Xfreeaddrinfo(ai);

	Xlisten(sock, 1);
	int new_sock = Xaccept(sock, NULL, NULL);
	if(new_sock < 0) {
		std::cout << "ERROR accepting client connection" << std::endl;
		return -1;
	}

	std::string response("Hello!");
	int rc = Xsend(new_sock, response.c_str(), response.size(), 0);
	if(rc == -1) {
		std::cout << "ERROR sending response to client" << std::endl;
		return -1;
	}
	if(rc != (int)response.size()) {
		std::cout << "ERROR entire response not sent" << std::endl;
		return -1;
	}

	Xclose(new_sock);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	Xclose(sock);
	return 0;
}
