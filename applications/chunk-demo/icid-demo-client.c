#include <Xsocket.h>
#include <dagaddr.hpp>
#include <publisher/publisher.h>
#include <headers/content_header.h>

#include <iostream>
#include <memory>
#include <atomic>

void usage(const char *progname)
{
	std::cout << "Usage: " << progname << " <CID_DAG>" << std::endl;
}

int main(int argc, char **argv)
{
	int sockfd;
	sockaddr_x sa;

	if(argc != 2) {
		usage(argv[0]);
		return 0;
	}

	// Read in the address and convert to a form recognized by bind
	Graph cid_dag(argv[1]);
	cid_dag.fill_sockaddr(&sa);

	// Create a socket and bind to it
	if((sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		std::cout << "Error creating socket" << std::endl;
		return -1;
	}
	if(Xbind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
		std::cout << "Error binding to socket" << std::endl;
		return -1;
	}
	if(Xlisten(sockfd, 1)) {
		std::cout << "Error listening to socket" << std::endl;
		return -1;
	}

	// Call XinterestedInCID for given address
	if(XinterestedInCID(sockfd, &sa)) {
		std::cout << "Error submitting interest" << std::endl;
		return -1;
	}

	// Now accept connections on the socket - for now just one
	sockaddr_x remote;
	socklen_t remote_len = sizeof(remote);
	int accepted_sock = Xaccept(sockfd, (sockaddr *)&remote, &remote_len);
	if(accepted_sock < 0) {
		std::cout << "Error accepting connection" << std::endl;
		return -1;
	}

	// Fetch the content
	// Receive chunk header and data
	std::string buf;
	std::unique_ptr<ContentHeader> chdr;
	std::atomic<bool> stop(false);
	if(xcache_get_content(accepted_sock, buf, chdr, stop)) {
		std::cout << "Error receiving a chunk" << std::endl;
		return -1;
	}
	std::cout << "Passed: content retrieved" << std::endl;

	return 0;
}
