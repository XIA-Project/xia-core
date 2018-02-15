// Project includes
#include "fs_fetch_request.h"
#include "fs_irq_table.h"

// XIA includes
#include "Xsocket.h"
#include "dagaddr.hpp"

// System includes
#include <assert.h>

FSFetchRequest::FSFetchRequest(InterestRequest &irq)
{
	_chunk_addr = irq.chunk_addr();
	_return_addr = irq.return_addr();
	_signature = irq.signature();
	_pool = FSThreadPool::get_pool();
	_irqtable = FSIRQTable::get_table();
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
	std::string cid = chunk_id();
	// Check if the chunk is locally available
	// If local, simply queue up a FSPushRequest for the chunk
	// See if the requested chunk is already listed in IRQ Table
	if(_irqtable->add_fetch_request(cid, _return_addr) == false) {

		std::cout << "Failed accounting for fetch request" << std::endl;
		return;
	}
	// If yes, append requestor to the list of entities requesting it
	// If not, create a new IRQ Table entry for this chunk

	return;
}

/*!
 * @brief get the ID of the chunk being fetched
 *
 * Note: can return an empty string if no chunk ID is found
 */
std::string FSFetchRequest::chunk_id()
{
	Graph g(_chunk_addr);
	std::string chunk_id = g.intent_CID_str();
	return chunk_id;
}
