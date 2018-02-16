// Project includes
#include "fs_push_request.h"
#include "fs_irq_table.h"

// XIA includes
#include "Xsocket.h"
#include "dagaddr.hpp"

// System includes
#include <assert.h>

FSPushRequest::FSPushRequest(std::string cid, std::string requestor)
{
	_cid.assign(cid);
	_requestor.assign(requestor);
	_pool = FSThreadPool::get_pool();
	_irqtable = FSIRQTable::get_table();
	if(XcacheHandleInit(&_xcache) < 0) {
		std::cout << "Failed talking to Xcache" << std::endl;
		throw "XcacheHandleInit failed";
	}
}

FSPushRequest::~FSPushRequest()
{
	XcacheHandleDestroy(&_xcache);
}

// Push a chunk that was requested by a client
void FSPushRequest::process()
{
	std::cout << "Pushing a chunk" << std::endl;

	// Build an address for the chunk - *->CID
	Node cidnode(_cid);
	Graph g(cidnode);
	sockaddr_x chunkaddr;
	g.fill_sockaddr(&chunkaddr);

	// Convert requestor address
	Graph rg(_requestor);
	sockaddr_x requestoraddr;
	rg.fill_sockaddr(&requestoraddr);

	if(XpushChunk(&_xcache, &chunkaddr, &requestoraddr) < 0) {
		std::cout << "Failed pushing chunk " << g.dag_string() << std::endl;
		// TODO: Send error by queuing FSErrorPushRequest
		return;
	}
	// Now queue up a task to clean up any state left over after a push
	/*
	FSWorkRequest *work = FSPushRequest(chunk_id(), _return_addr);
	_pool->queue_work(work);
	*/

	return;
}
