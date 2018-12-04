#include <iostream>

#include <cpr/cpr.h>
#include <json.hpp>

#include "Xsocket.h"
#include "dagaddr.hpp"

int connect_to(std::string dag_str)
{
	std::cout << "Creating a socket" << std::endl;
	auto sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if(sockfd < 0) {
		std::cout << "ERROR creating socket" << std::endl;
		return -1;
	}

	Graph g(dag_str);

	// Convert addr into sockaddr for Xconnect
	sockaddr_x sa;
	g.fill_sockaddr(&sa);

	// Then connect to that address
	if(Xconnect(sockfd, (struct sockaddr *)&sa, sizeof(sockaddr_x)) < 0) {
		std::cout << "ERROR connecting to server" << std::endl;
		return -1;
	}
	Xclose(sockfd);
	return 0;
}

int main()
{

	std::cout << "Getting server address" << std::endl;
	auto response = cpr::Get(cpr::Url{"172.16.252.131:24703/GNS/read?guid=70FD73C018FAEED5F041AACB9BA28BCB651A0F2B&field=demoserveraddr"});
	std::cout << "Request status: " << response.status_code << std::endl;
	std::cout << "Server addrs: " << response.text << std::endl;

	auto json = nlohmann::json::parse(response.text);
	if(json.empty()) {
		std::cout << "ERROR no response from GNS query" << std::endl;
		return -1;
	}
	for(nlohmann::json::iterator it=json.begin(); it!=json.end(); ++it) {
		std::string addr = it.value()[0].get<std::string>();
		std::cout << "Instance: " << it.key() << " Addr: " << addr << std::endl;
		if(connect_to(addr)) {
			std::cout << "ERROR connecting to " << addr << std::endl;
			return -1;
		}
	}
	std::cout << "PASSED connecting to multiple servers" << std::endl;

	return 0;
}
