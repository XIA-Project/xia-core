// Project includes
#include "fs_fetch_request.h"
#include "fs_push_request.h"
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
	if(XcacheHandleInit(&_xcache) < 0) {
		std::cout << "Failed talking to Xcache" << std::endl;
		throw "XcacheHandleInit failed";
	}
}

FSFetchRequest::~FSFetchRequest()
{
	XcacheHandleDestroy(&_xcache);
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
	// If yes, append requestor to the list of entities requesting it
	// If not, create a new IRQ Table entry for this chunk
	if(_irqtable->add_fetch_request(cid, _return_addr) == false) {

		std::cout << "Failed accounting for fetch request" << std::endl;
		return;
	}
	// The chunk is not local, so fetch it. Synchronously for now.
	// TODO: Do asynchronous fetch to avoid blocking a thread
	Graph g(_chunk_addr);
	sockaddr_x addr;
	g.fill_sockaddr(&addr);

	// TODO: Do we need to guard against multiple threads fetching a chunk?
	// I believe xcache deduplicates the requests but confirm
	void *buf;
	if(XfetchChunk(&_xcache, &buf, 0, &addr, sizeof(addr)) < 0) {
		std::cout << "Failed fetching chunk " << _chunk_addr << std::endl;
		// TODO: Send error by queuing FSErrorPushRequest
		// TODO: Remove CID from FSIRQTable also
		return;
	}
	// We don't need the chunk contents. It just needs to be in local cache
	free(buf);

	// Get the list of requestors for this chunk and remove them from table
	RequestorList requestors = _irqtable->requestors(cid);
	RequestorList::iterator it;
	for(it=requestors.begin(); it!=requestors.end(); it++) {

		// Now queue up a task to push the chunk back to each requestor
		FSWorkRequest *work = new FSPushRequest(chunk_id(), *it);
		_pool->queue_work(work);

	}

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
