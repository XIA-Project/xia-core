#include <iostream>
#include <string>
#include <random>
#include <atomic>

#include <signal.h>
#include <unistd.h>

#include <cpr/cpr.h>

#include "Xsocket.h"
#include "Xkeys.hh"
#include "dagaddr.hpp"

std::atomic<bool> stop(false);

void work(int sockfd, std::string their_addr)
{
	std::cout << "New connection from: "
		<< their_addr << std::endl;
	Xclose(sockfd);
}

void sigint_handler(int)
{
	stop.store(true);
}

int main()
{
	SIDKey sid;
	std::random_device randev;
	std::mt19937 mt(randev());
	std::uniform_int_distribution<> dist(1, 100);
	int identifier = dist(mt);

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint_handler;
	sigfillset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	std::cout << "Creating a socket" << std::endl;
	auto sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if(sockfd < 0) {
		std::cout << "ERROR creating socket" << std::endl;
		return -1;
	}

	std::cout << "Getting our address" << std::endl;
	struct addrinfo hints, *ai;
	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_XIA;
	if(Xgetaddrinfo(NULL, sid.to_string().c_str(), &hints, &ai) != 0) {
		std::cout << "Failed getting address" << std::endl;
		// TODO: remove temporary SID
		return -1;
	}

	sockaddr_x addr;
	memcpy(&addr, ai->ai_addr, sizeof(addr));
	Xfreeaddrinfo(ai);
	Graph our_addr(&addr);

	std::cout << "Taking ownership of address" << std::endl;
	if(Xbind(sockfd, (struct sockaddr *)&addr, sizeof(sockaddr_x)) < 0) {
		std::cout << "ERROR binding to socket" << std::endl;
		// TODO: remove SID
		return -1;
	}
	Xlisten(sockfd, 5);

	std::cout << "Registering our address with GNS" << std::endl;
	std::string gns_url("localhost:5678/GNS/");
	std::string cmd = gns_url + "create?";
	cmd += "guid=70FD73C018FAEED5F041AACB9BA28BCB651A0F2B";
	cmd += "&field=demoserveraddr";
	cmd += "." + std::to_string(identifier);
	cmd += "&value=" + our_addr.http_url_string();
	std::cout << "Command: " << cmd << std::endl;
	
	auto response = cpr::Get(cpr::Url{cmd});
	std::cout << "Response status code: " << response.status_code << std::endl;
	std::cout << response.text << std::endl;

	std::cout << "Waiting for connections" << std::endl;
	while(true) {
		// Wait for an incoming connection and handle SIGINT
		struct pollfd ufd;
		ufd.fd = sockfd;
		ufd.events = POLLIN;
		unsigned nfds = 1;
		int timeout = 500;	// milliseconds
		int rc = Xpoll(&ufd, nfds, timeout);
		if(rc == 0) {
			if(stop.load()) {
				break;
			}
			continue;
		}
		if(rc < 0) {
			std::cout << "ERROR waiting for incoming connections" << std::endl;
			break;
		}

		int newsock;
		sockaddr_x their_addr;
		socklen_t sz = sizeof(their_addr);
		if((newsock = Xaccept(sockfd, (sockaddr *)&their_addr, &sz)) < 0) {
			std::cout << "ERROR accepting connection" << std::endl;
			continue;
		}
		Graph g(&their_addr);
		auto t = std::thread(work, newsock, g.dag_string());
		t.detach();
	}
	std::cout << "Unregistering our address from GNS" << std::endl;
	cmd = gns_url + "removefield?";
	cmd += "guid=70FD73C018FAEED5F041AACB9BA28BCB651A0F2B";
	cmd += "&field=demoserveraddr";
	cmd += "." + std::to_string(identifier);
	std::cout << "Command: " << cmd << std::endl;

	response = cpr::Get(cpr::Url{cmd});
	std::cout << "Response status code: " << response.status_code << std::endl;
	std::cout << response.text << std::endl;

	Xclose(sockfd);
	return 0;
}
