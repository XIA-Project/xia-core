// Project includes
#include "icid_push_request.h"
#include "icid_irq_table.h"

// XIA includes
#include "Xsocket.h"
#include "dagaddr.hpp"

// System includes
#include <assert.h>

ICIDPushRequest::ICIDPushRequest(std::string cid, std::string requestor)
{
	_cid.assign(cid);
	_requestor.assign(requestor);
	_pool = ICIDThreadPool::get_pool();
	_irqtable = ICIDIRQTable::get_table();
	if(XcacheHandleInit(&_xcache) < 0) {
		std::cout << "Failed talking to Xcache" << std::endl;
		throw "XcacheHandleInit failed";
	}
}

ICIDPushRequest::~ICIDPushRequest()
{
	XcacheHandleDestroy(&_xcache);
}

// Push a chunk that was requested by a client
void ICIDPushRequest::process()
{
	std::cout << "Pushing a chunk" << std::endl;

	// Build an address for the chunk - *->CID
	Node src;
	Node cidnode(_cid);
	Graph g = src * cidnode;
	sockaddr_x chunkaddr;
	g.fill_sockaddr(&chunkaddr);

	// Convert requestor address
	Graph rg(_requestor);
	sockaddr_x requestoraddr;
	rg.fill_sockaddr(&requestoraddr);

	std::cout << "XpushChunk " << g.dag_string() << " to "
		<< rg.dag_string() << " called" << std::endl;
	if(XpushChunk(&_xcache, &chunkaddr, &requestoraddr) < 0) {
		std::cout << "Failed pushing chunk " << g.dag_string() << std::endl;
		// TODO: Send error by queuing ICIDErrorPushRequest
		return;
	}
	return;
}
