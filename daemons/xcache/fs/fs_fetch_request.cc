// Project includes
#include "fs_fetch_request.h"

// XIA includes
#include "Xsocket.h"

// System includes
#include <assert.h>

FSFetchRequest::FSFetchRequest(InterestRequest &irq)
{
	_chunk_addr = irq.chunk_addr();
	_return_addr = irq.return_addr();
	_signature = irq.signature();
	_pool = FSThreadPool::get_pool();
}

FSFetchRequest::~FSFetchRequest()
{
}

FSFetchRequest *FSFetchRequest::from_client(std::string &buf)
{
	InterestRequest irq_buf;

	// Retrieve the client request from the protobuf
	if(irq_buf.ParseFromString(buf) == false) {
		std::cout << "Invalid InterestRequest from client" << std::endl;
		return nullptr;
	}

	// Build a Fetch Request from the client request
	FSFetchRequest *req = new FSFetchRequest(irq_buf);
	return req;
}

// ASSUME: An accepted socket for a fetch connection is available
// Interact with the fetch and retrieve their request for content
void FSFetchRequest::process()
{
	std::cout << "Fetching a chunk" << std::endl;

	return;
}
