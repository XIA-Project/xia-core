#include <iostream>

#include <cpr/cpr.h>

#include "XIAStreamSocket.hh"
#include "dagaddr.hpp"

int main()
{
	// Query GNS for the server address
	std::cout << "Getting server address" << std::endl;
	auto response = cpr::Get(cpr::Url{"172.16.252.131:24703/GNS/read?guid=70FD73C018FAEED5F041AACB9BA28BCB651A0F2B&field=demoserveraddr"});
	std::cout << "Request status: " << response.status_code << std::endl;
	std::cout << "Server addr: " << response.text << std::endl;

	// Parse server address
	std::string addr_json = response.text;
	std::string addr = addr_json.substr(2, addr_json.size()-4);
	std::cout << "Address received: " << addr << std::endl;
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

	std::cout << "PASSED gns query for server and connect to it" << std::endl;

	// sock goes out of scope and is closed automatically
	return 0;
}
