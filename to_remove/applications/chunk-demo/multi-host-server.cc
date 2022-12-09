#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"

#include <iostream>
#include <thread>
#include <chrono>

#define SERVER1 "multi-host-server1.xia"
#define SERVER "multi-host-server.xia"

int main()
{
	int sock;

	// Get server1's address. Don't start anything until we find it.
	struct addrinfo *server1_ai;
	if(Xgetaddrinfo(SERVER1, NULL, NULL, &server1_ai) != 0) {
		std::cout << "ERROR getting server1 addr (not running?)" << std::endl;
		return -1;
	}
	sockaddr_x *server1_addr = (sockaddr_x *)server1_ai->ai_addr;
	Graph server1_dag(server1_addr);
	std::cout << "Server 1: " << server1_dag.dag_string() << std::endl;

	if((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		std::cout << "Error creating socket to communicate" << std::endl;
		return -1;
	}

	// Now let's start our own service
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
	Graph server2_dag(dag);
	std::cout << "Server 2: " << server2_dag.dag_string() << std::endl;

	if(Xbind(sock, (struct sockaddr *)dag, sizeof(sockaddr_x)) < 0) {
		Graph addr((sockaddr_x *) ai->ai_addr);
		std::cout << "ERROR binding to " << addr.dag_string() << std::endl;
		return -1;
	}

	// TODO:
	// server1_dag has the original server's dag: AD1 -> HID1 -> SID1
	// server2_dag has the fallback server's dag: AD2 -> HID2 -> SID2
	//
	// The combined dag should look like
	// *-> AD1 -> HID1 ----------------> SID1
	//            '-> AD2 -> HID2 -> SID2 -'
	//
	// We can make it happen by adding a node from intent AD
	// to intent AD, HID, SID of second DAG, assuming it is a simple dag
	if(server1_dag.merge_multi_host_fallback(server2_dag)) {
		std::cout << "ERROR adding fallback" << std::endl;
		return -1;
	}
	std::cout << "Combined address: " << server1_dag.dag_string() << std::endl;
	sockaddr_x combined_addr;
	server1_dag.fill_sockaddr(&combined_addr);
	if(XregisterName(SERVER, &combined_addr) < 0) {
		std::cout << "ERROR sending our addr to nameserver" << std::endl;
		return -1;
	}
	std::cout << "Registered combined address with nameservice" << std::endl;

	Xfreeaddrinfo(server1_ai);
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
