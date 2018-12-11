#include "gns-demo.hh"

#include <iostream>
#include <atomic>

#include <signal.h>
#include <unistd.h>

#include "Xsocket.h"
#include "Xgns.h"
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
	GNSServer gns(PUBLISHER_NAME);

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
	std::string servname = SERVER_NAME + ".fixed";
	if(gns.makeTempEntry(servname, our_addr.http_url_string())==false){
		std::cout << "ERROR creating GNS entry for server" << std::endl;
		return -1;
	}

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

	// GNSServer goes out of scope, all temp entries will be removed
	// sock goes out of scope. It will be closed and temporary SID keys deleted
	return 0;
}
