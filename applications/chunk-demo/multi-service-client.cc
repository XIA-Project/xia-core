#include "Xsocket.h"
#include "dagaddr.hpp"

#include <iostream>
#include <chrono>
#include <thread>

#define SERVER_NAME "multi-service-server.xia"
#define SERVER_NAME_2 "multi-service-server-2.xia"

int connect_and_receive(sockaddr_x *sa)
{
	Graph g(sa);

	// Create a socket to connect to server
	int sock;
	if((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		std::cout << "ERROR creating socket to talk to server" << std::endl;
		return -1;
	}

	if(Xconnect(sock, (struct sockaddr*) sa, sizeof(sockaddr_x)) < 0) {
		std::cout << "ERROR connecting to server: " <<
			g.dag_string() << std::endl;
		return -1;
	}
	std::cout << "Connected successfully to server" << std::endl;

	size_t buflen = 256;
	char buf[buflen];
	int rc = Xrecv(sock, buf, buflen, 0);
	if(rc < 0) {
		std::cout << "ERROR getting response from server" << std::endl;
		return -1;
	}
	std::cout << "Server says: " << std::string(buf, rc) << std::endl;
	Xclose(sock);
	return 0;
}

int main()
{
	// Get the address for server from the name service
	struct addrinfo *ai;
	if(Xgetaddrinfo(SERVER_NAME, NULL, NULL, &ai) != 0) {
		std::cout << "ERROR getting server address" << std::endl;
		return -1;
	}

	sockaddr_x *sa = (sockaddr_x *)ai->ai_addr;
	Graph g(sa);
	std::cout << "Server addr: " << g.dag_string() << std::endl;

	if(connect_and_receive(sa)) {
		std::cout << "ERROR connecting to server: " <<
			g.dag_string() << std::endl;
		return -1;
	}
	std::cout << "Connected successfully to server once" << std::endl;

	// Wait for the server to close the intent SID in combined DAG
	std::this_thread::sleep_for(std::chrono::seconds(3));

	if(connect_and_receive(sa)) {
		std::cout << "ERROR connecting to server again: "
			<< g.dag_string() << std::endl;
		return -1;
	}
	std::cout << "PASSED: Connected to server again" << std::endl;

	Xfreeaddrinfo(ai);

	return 0;
}
