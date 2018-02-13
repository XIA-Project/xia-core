// Project includes
#include "fs_client_request.h"

// XIA includes
#include "Xsocket.h"

// System includes
#include <assert.h>

FSClientRequest::FSClientRequest(int sockfd)
{
	_sockfd = sockfd;
	_pool = FSThreadPool::get_pool();
}

FSClientRequest::~FSClientRequest()
{
	Xclose(_sockfd);
}

// ASSUME: An accepted socket for a client connection is available
// Interact with the client and retrieve their request for content
void FSClientRequest::process()
{
	std::cout << "Handling client connection" << std::endl;

	// Wait for data from client
	// NOTE: _sockfd is closed by the destructor
	int nfds = 0;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(_sockfd, &fds);

	struct timeval tv;
	tv.tv_sec = FS_CLIENT_TIMEOUT;
	tv.tv_usec = 0;

	if((nfds = Xselect(_sockfd+1, &fds, NULL, NULL, &tv)) < 0) {
		std::cout << "Failed waiting for client data" << std::endl;
		return;
	} else if (nfds == 0) {
		std::cout << "Timed out waiting for client" << std::endl;
		return;
	} else if (!FD_ISSET(_sockfd, &fds)) {
		std::cout << "No data but Xselect said there should be" << std::endl;
		return;
	}

	char buf[FS_CLIENT_MAXBUF];
	// size of client request - first 4 bytes in network byte order
	uint32_t req_len;
	if(Xrecv(_sockfd, &req_len, sizeof(req_len), 0) != sizeof(req_len)) {
		std::cout << "Failed getting client request size" << std::endl;
		return;
	}
	// convert to host byte order
	req_len = ntohl(req_len);
	std::cout << "Fetching client request of size " << req_len << std::endl;

	uint32_t count;
	uint32_t remaining = req_len;
	uint32_t offset = 0;
	assert(remaining < sizeof(buf));

	// Retrieve the entire request
	// TODO: verify logic below
	while(remaining > 0) {
		count = Xrecv(_sockfd, &buf[offset], sizeof(buf)-offset, 0);
		std::cout << "Got data of size " << count << std::endl;
		if(count == 0) {
			break;
		}
		remaining -= count;
		offset += count;
	}
	if(remaining != 0) {
		std::cout << "Entire client request not received" << std::endl;
		return;
	}
	// Retrieve interest request from client protobuf
	// and convert it into a request to fetch the actual chunk
	/*
	FSWorkRequest *work = FSFetchRequest::from_client(buf, req_len);
	if(work == nullptr) {
		std::cout << "Bad request from client" << std::endl;
		return;
	}
	_pool->queue_work(work);
	*/

	// Validate the request
	// Queue up a fetch request to initiate the fetch
	return;
}
