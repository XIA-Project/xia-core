#include <iostream>
#include <atomic>

#include <signal.h>
#include <unistd.h>

#include <cpr/cpr.h>

#include "Xsocket.h"
#include "dagaddr.hpp"
#include "XIAStreamSocket.hh"

std::atomic<bool> stop(false);

void sigint_handler(int)
{
	stop.store(true);
}

void work(std::unique_ptr<XIAStreamSocket> sock, std::string their_addr)
{
	std::cout << "New connection from: "
		<< their_addr << std::endl;
	// sock goes out of scope and will be closed automatically
}

int main()
{
	SIDKey sid;

	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = sigint_handler;
	sigfillset(&action.sa_mask);
	sigaction(SIGINT, &action, NULL);

	// Create a server socket
	XIAStreamSocket sock;

	sockaddr_x servaddr;
	if(sock.bind_and_listen_on_temp_sid(5, servaddr)) {
		std::cout << "ERROR creating server socket" << std::endl;
		return -1;
	}
	Graph our_addr(&servaddr);

	// Register server address on GNS
	std::cout << "Registering our address with GNS" << std::endl;
	std::string gns_url("localhost:5678/GNS/");
	std::string cmd = gns_url + "create?";
	cmd += "guid=70FD73C018FAEED5F041AACB9BA28BCB651A0F2B";
	cmd += "&field=demoserveraddr";
	cmd += "&value=" + our_addr.http_url_string();
	std::cout << "Command: " << cmd << std::endl;
	
	auto response = cpr::Get(cpr::Url{cmd});
	std::cout << "Response status code: " << response.status_code << std::endl;
	std::cout << response.text << std::endl;

	std::cout << "Waiting for connections" << std::endl;

	// Set up for polling on sockfd to allow for graceful interruption
	struct pollfd ufd;
	ufd.fd = sock.fd();	// TODO: in future fd() may not be available
	ufd.events = POLLIN;
	unsigned nfds = 1;
	int timeout = 500;	// milliseconds

	while(true) {
		int rc = Xpoll(&ufd, nfds, timeout);
		if(rc <= 0) {
			if(stop.load()) {
				std::cout << "Interrupted. Cleaning up" << std::endl;
				break;
			}
			continue;
		}

		// Accept an incoming connection from a client
		sockaddr_x their_addr;
		auto newsock = sock.accept(&their_addr);
		Graph g(&their_addr);

		// Hand off to a worker thread
		auto t = std::thread(work, std::move(newsock), g.dag_string());
		t.detach();
	}

	std::cout << "Unregistering our address from GNS" << std::endl;
	cmd = gns_url + "removefield?";
	cmd += "guid=70FD73C018FAEED5F041AACB9BA28BCB651A0F2B";
	cmd += "&field=demoserveraddr";
	std::cout << "Command: " << cmd << std::endl;

	response = cpr::Get(cpr::Url(cmd));
	std::cout << "Response status code: " << response.status_code << std::endl;
	std::cout << response.text << std::endl;

	// sock goes out of scope. It will be closed and temporary SID keys deleted
	return 0;
}
