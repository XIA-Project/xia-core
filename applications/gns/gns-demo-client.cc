#include "gns-demo.hh"

#include <iostream>
#include "Xgnsclient.h"
#include "XIAStreamSocket.hh"
#include "dagaddr.hpp"

int main()
{
	char rootdir[2048];
	if(XrootDir(rootdir, sizeof(rootdir)) == NULL) {
		std::cout << "ERROR: finding XIA working directory" << std::endl;
		return -1;
	}
	std::string conf_file(rootdir);
	conf_file += "/" + GNS_DEMO_PATH + "/gns-conf.json";
	GNSClient gns(conf_file);


	// Query GNS for the server address
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
		std::cout << "Creating socket" << std::endl;
		XIAStreamSocket sock;
		std::cout << "Connecting to server " << g.dag_string() << std::endl;
		if(sock.connect(&sa)) {
			std::cout << "ERROR connecting to server" << std::endl;
			return -1;
		}

		// Send a message to the server
		std::cout << "Sending message to server" << std::endl;
		std::string msg("Hello");
		int sent = sock.send(msg.c_str(), msg.size());
		if(sent != (int)msg.size()) {
			std::cout << "ERROR sending message to server" << std::endl;
			return -1;
		}
		char buf[1024];
		int received = sock.recv(buf, sizeof(buf));
		if(received != (int)msg.size()) {
			std::cout << "ERROR got incorrect data" << std::endl;
			return -1;
		}
		// sock goes out of scope and is closed automatically
	}

	std::cout << "PASSED gns query for server and connect to it" << std::endl;

	return 0;
}
