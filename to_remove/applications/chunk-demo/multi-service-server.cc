#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"

#include <iostream>
#include <future>	// std::async
#include <chrono>	// std::chrono::seconds

#define NAME "multi-service-server.xia"
#define NAME2 "multi-service-server-2.xia"

// Accept a connection on given socket
// Send a dummy response and end the task
int handle_connection(int sock, Graph addr)
{
	Xlisten(sock, 1);
	int new_sock = Xaccept(sock, NULL, NULL);
	if(new_sock < 0) {
		std::cout << "Error accepting connection for addr "
			<< addr.dag_string() << std::endl;
		return -1;
	}
	std::cout << "Accepted connection on " << addr.dag_string() << std::endl;
	std::string response("Hello!");
	int rc = Xsend(new_sock, response.c_str(), response.size(), 0);
	if(rc == -1) {
		std::cout << "Error sending response to client" << std::endl;
	}
	if(rc != (int)response.size()) {
		std::cout << "Error entire response not sent to client" << std::endl;
	}
	Xclose(new_sock);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	Xclose(sock);
	return 0;
}

int main()
{
	int sock1, sock2;
	std::string sid1, sid2;
	sockaddr_x *dag;

	// Start two services and create a combined DAG
	if((sock1 = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		std::cout << "Unable to create first socket" << std::endl;
		return -1;
	}
	if((sock2 = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		std::cout << "Unable to create second socket" << std::endl;
		return -1;
	}

	size_t sid_strlen = strlen("SID:") + XIA_SHA_DIGEST_STR_LEN;
	char sid_string[sid_strlen];

	// Create first SID
	if(XmakeNewSID(sid_string, sid_strlen)) {
		std::cout << "Unable to create new SID for first socket" << std::endl;
		return -1;
	}
	sid1.assign(sid_string, strlen(sid_string));
	std::cout << "First SID: " << sid1 << std::endl;

	// Create second SID
	bzero(sid_string, sid_strlen);
	if(XmakeNewSID(sid_string, sid_strlen)) {
		std::cout << "Unable to create new SID for first socket" << std::endl;
		return -1;
	}
	sid2.assign(sid_string, strlen(sid_string));
	std::cout << "Second SID: " << sid2 << std::endl;

	// Address to bind to for first SID
	struct addrinfo *ai;
	if(Xgetaddrinfo(NULL, sid1.c_str(), NULL, &ai) != 0) {
		std::cout << "Unable to get address for " << sid1 << std::endl;
		return -1;
	}
	Graph addr1((sockaddr_x *)ai->ai_addr);
	dag = (sockaddr_x *)ai->ai_addr;
	if(Xbind(sock1, (struct sockaddr *)dag, sizeof(sockaddr_x)) < 0) {
		Xclose(sock1);
		std::cout << "Failed binding to " << addr1.dag_string() << std::endl;
		return -1;
	}
	Xfreeaddrinfo(ai);


	// Address to bind to for second SID
	if(Xgetaddrinfo(NULL, sid2.c_str(), NULL, &ai) != 0) {
		std::cout << "Unable to get address for " << sid1 << std::endl;
		return -1;
	}
	Graph addr2((sockaddr_x *)ai->ai_addr);
	dag = (sockaddr_x *)ai->ai_addr;
	if(Xbind(sock2, (struct sockaddr *)dag, sizeof(sockaddr_x)) < 0) {
		Xclose(sock1);
		std::cout << "Failed binding to " << addr2.dag_string() << std::endl;
		return -1;
	}
	Xfreeaddrinfo(ai);

	// Start accepting connections on both sockets
	auto sock1_work = std::async(std::launch::async,
			handle_connection, sock1, addr1);
	auto sock2_work = std::async(std::launch::async,
			handle_connection, sock2, addr2);

	// Address with SID2 as fallback for SID1
	Graph addr = addr1;
	if(addr.add_sid_fallback(sid2)) {
		std::cout << "Unable to add fallback " << sid2 << std::endl;
		return -1;
	}

	std::cout << "Combined address: " << addr.dag_string()
		<< std::endl;

	// Register combined address for client to find
	sockaddr_x combined_addr;
	addr.fill_sockaddr(&combined_addr);
	if(XregisterName(NAME, &combined_addr) < 0) {
		std::cout << "Unable to register name" << std::endl;
		return -1;
	}

	// Now wait until the client connects twice to the combined address
	if(sock1_work.get() == -1) {
		std::cout << "Error handling socket 1" << std::endl;
	}
	std::cout << "Connection to sock1 completed" << std::endl;

	if(sock2_work.get() == -1) {
		std::cout << "Error handling socket 2" << std::endl;
	}
	std::cout << "Connection to sock2 completed" << std::endl;

	if(XremoveSID(sid1.c_str())) {
		std::cout << "Failed removing " << sid1 << std::endl;
	}
	if(XremoveSID(sid2.c_str())) {
		std::cout << "Failed removing " << sid2 << std::endl;
	}
	return 0;
}
