#include "gns-demo.hh"

#include <iostream>

#include "Xgnsclient.h"
#include "XIAStreamSocket.hh"
#include "dagaddr.hpp"

int connect_to(std::string dag_str)
{
	// Convert server address to a sockaddr_x
	Graph g(dag_str);
	sockaddr_x sa;
	g.fill_sockaddr(&sa);

	// Then connect to that address
	XIAStreamSocket sock;
	if(sock.connect(&sa)) {
		std::cout << "ERROR connecting to " << dag_str << std::endl;
		return -1;
	}
	// sock goes out of scope and is closed automatically
	return 0;
}

int main()
{
	// Query GNS for all matching server addresses
	GNSClient gns(PUBLISHER_NAME, "172.16.252.131:24703");
	std::vector<std::string> addrs;

	addrs = gns.queryEntry(SERVER_NAME);

	for (auto addr : addrs) {
		if(connect_to(addr)) {
			std::cout << "ERROR connecting to " << addr << std::endl;
			return -1;
		}
	}
	std::cout << "PASSED connecting to multiple servers" << std::endl;

	return 0;
}
