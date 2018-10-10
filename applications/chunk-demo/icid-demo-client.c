#include <Xkeys.h>
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
	sockaddr_x cidaddr;
	char sid_string[XIA_XID_STR_SIZE];

	if(argc != 2) {
		usage(argv[0]);
		return 0;
	}

	// Read in the address and convert to a form recognized by bind
	Graph cid_dag(argv[1]);
	cid_dag.fill_sockaddr(&cidaddr);

	// Create a socket
	if((sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		std::cout << "Error creating socket" << std::endl;
		return -1;
	}

	// with a temporary SID
	bzero(sid_string, sizeof(sid_string));
	if(XmakeNewSID(sid_string, sizeof(sid_string))) {
		std::cout << "Error making new SID" << std::endl;
		return -1;
	}

	// used in a temporary address
	struct addrinfo *ai;
	if(Xgetaddrinfo(NULL, sid_string, NULL, &ai)) {
		std::cout << "Error getting address" << std::endl;
		return -1;
	}

	// Bind our server socket to the temporary address
	sockaddr_x bind_addr;
	memcpy(&bind_addr, (sockaddr_x *)ai->ai_addr, sizeof(sockaddr_x));
	if(Xbind(sockfd, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
		std::cout << "Error binding to socket" << std::endl;
		return -1;
	}
	// The socket now listens for incoming push requests
	if(Xlisten(sockfd, 1)) {
		std::cout << "Error listening to socket" << std::endl;
		return -1;
	}

	// Call XinterestedInCID for given address
	if(XinterestedInCID(sockfd, &cidaddr)) {
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

	Xclose(accepted_sock);
	Xclose(sockfd);
	Xfreeaddrinfo(ai);
	XremoveSID(sid_string);

	return 0;
}
