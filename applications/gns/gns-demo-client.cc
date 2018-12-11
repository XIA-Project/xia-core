#include "gns-demo.hh"

#include <iostream>
#include "Xgnsclient.h"
#include "XIAStreamSocket.hh"
#include "dagaddr.hpp"

int main()
{
	// Query GNS for the server address
	GNSClient gns(PUBLISHER_NAME, "172.16.252.131:24703");
	std::vector<std::string> addrs;
	addrs = gns.queryEntry(SERVER_NAME);
	if(addrs.size() != 1) {
		std::cout << "Expected 1 addr. Got " << addrs.size() << std::endl;
		return -1;
	}
	for (auto addr : addrs) {
		Graph g(addr);

		// Convert addr into sockaddr for Xconnect
		sockaddr_x sa;
		g.fill_sockaddr(&sa);

		// Now, create an XIA socket and connect to the server
		XIAStreamSocket sock;
		if(sock.connect(&sa)) {
			std::cout << "ERROR connecting to server" << std::endl;
			return -1;
		}
		// sock goes out of scope and is closed automatically
	}

	std::cout << "PASSED gns query for server and connect to it" << std::endl;

	return 0;
}
