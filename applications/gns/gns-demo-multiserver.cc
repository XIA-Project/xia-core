#include <iostream>
#include <string>
#include <random>
#include <atomic>

#include <signal.h>
#include <unistd.h>

#include <cpr/cpr.h>

#include "Xsocket.h"
#include "dagaddr.hpp"
#include "XIAStreamSocket.hh"

std::atomic<bool> stop(false);

void work(std::unique_ptr<XIAStreamSocket> sock, std::string their_addr)
{
	std::cout << "New connection from: "
		<< their_addr << std::endl;
	// sock goes out of scope and will be closed automatically
}

void sigint_handler(int)
{
	stop.store(true);
}

int main()
{
	std::random_device randev;
	std::mt19937 mt(randev());
	std::uniform_int_distribution<> dist(1, 100);
	int identifier = dist(mt);

	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint_handler;
	sigfillset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);

	XIAStreamSocket sock;
	sockaddr_x servaddr;
	if(sock.bind_and_listen_on_temp_sid(5, servaddr)) {
		std::cout << "ERROR creating server socket" << std::endl;
		return -1;
	}
	Graph our_addr(&servaddr);

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

	struct pollfd ufd;
	ufd.fd = sock.fd();	// TODO: fd() may not be available in future
	ufd.events = POLLIN;
	unsigned nfds = 1;
	int timeout = 500;	// milliseconds
	std::cout << "Waiting for connections" << std::endl;
	while(true) {
		// Wait for an incoming connection and handle SIGINT
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

		sockaddr_x their_addr;
		auto newsock = sock.accept(&their_addr);
		Graph g(&their_addr);
		auto t = std::thread(work, std::move(newsock), g.dag_string());
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

	// sock goes out of scope. It is closed and the temporary SID keys deleted.
	return 0;
}
