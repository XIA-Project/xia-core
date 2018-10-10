// Monitor incoming ICID requests
//
// Launch a thread to listen on a socket
// The socket receives RAW packets containing XIA header only
// Retrieve ICID from the XIA Header dest dag
// Check if corresponding CID is local
// Serve if local, add to interest table if not.
// Create new Xsocket to receive chunk, bind to a new address
// Send new interest if not local, with newly created socket address as source
// Add socket to Xselect loop listening for incoming packets
// Xselect loop will start a new thread to receive the chunk contents.
// The same thread can schedule xcache_push_chunk requests for requestors
//
// Questions:
// How does this interact if the same chunk is requested directly by an app?
// - Maybe have Xcache do the fetch. It avoids duplicate requests already!
//   - but that would require fetch vs. push.

// ICID/Xcache includes
#include "icid_monitor.h"
#include "icid_thread_pool.h"
#include "icid_push_request.h"
#include "icid_receive_request.h"

// XIA standard includes
#include <Xkeys.h>             // XmakeNewSID
#include <dagaddr.hpp>
#include <clicknet/xia.h>
#include <Xsocket.h>

// C++ includes
#include <iostream>

// For UNIX datagram socket
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define ICID_SOCK_NAME "/tmp/xcache-icid.sock"
#define ICID_MAXBUF 2048

ICIDMonitor::ICIDMonitor()
{
	// Get references to controller, thread pool and interest table
	ctrl = xcache_controller::get_instance();
	pool = ICIDThreadPool::get_pool();
	irqtable = ICIDIRQTable::get_table();

	// Initialize variables to invalid values
	recv_sock = -1;
	icid_sock = -1;
	sid_string_initialized = false;

	// A socket to listen for chunks pushed in response to our requests
	if((recv_sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		std::cout << "Error creating socket for pushed chunks" << std::endl;
		throw "Error creating socket to receive pushed chunks";
	}
	if(XmakeNewSID(sid_string, sizeof(sid_string))) {
		std::cout << "Error making new SID for interest CIDs" << std::endl;
		throw "Unable to make new SID";
	}
	sid_string_initialized = true;
	struct addrinfo *ai;
	if(Xgetaddrinfo(NULL, sid_string, NULL, &ai)) {
		std::cout << "Error building address for recv socket" << std::endl;
		throw "Error getting address for pushed chunk socket";
	}
	memcpy(&recv_addr, (sockaddr_x *)ai->ai_addr, sizeof(sockaddr_x));
	Xfreeaddrinfo(ai);
	if(Xbind(recv_sock,
				(struct sockaddr *)&recv_addr, sizeof(sockaddr_x)) < 0) {
		throw "Error binding to socket for pushed chunks";
	}
	if(Xlisten(recv_sock, 10)) {
		throw "Error listening to pushed chunks socket";
	}

	// A socket to receive incoming Interest packets
	if((icid_sock = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
		std::cout << "Error creating socket for Interest CIDs" << std::endl;
		throw "Unable to create socket for Interest CIDs";
	}
	sockaddr_un sa;
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strcpy(sa.sun_path, ICID_SOCK_NAME);

	if(bind(icid_sock, (struct sockaddr *)&sa, sizeof(sa))) {
		std::cout << "Error binding to Interest CID socket" << std::endl;
		throw "Unable to bind to Interest CID socket";
	}
}

ICIDMonitor::~ICIDMonitor()
{
	if(icid_sock != -1) {
		close(icid_sock);
	}
	if(recv_sock != -1) {
		Xclose(recv_sock);
	}

	if(sid_string_initialized) {
		XremoveSID(sid_string);
	}
}

void ICIDMonitor::handle_ICID_packet()
{
	char buffer[ICID_MAXBUF];
	int ret;
	bool fetch_needed = false;

	ret = recvfrom(icid_sock, buffer, ICID_MAXBUF, 0, NULL, NULL);
	if(ret <= 0) {
		std::cout << "Error reading interest:"
			<< strerror(errno) << std::endl;
		return;
	}

	// Read in the XIA Header here
	// TODO: Add additional checks to ensure buffer contains XIA pkt
	struct click_xia *xiah = (struct click_xia *)buffer;
	Graph dst_dag;
	dst_dag.from_wire_format(xiah->dnode, &xiah->node[0]);

	Graph src_dag;
	src_dag.from_wire_format(xiah->snode, &xiah->node[xiah->dnode]);

	// Now find the ICID intent
	// TODO: Handle exception thrown if intent is not an ICID
	Node icid = dst_dag.intent_ICID();

	// Convert into CID to check for
	Node cid(XID_TYPE_CID, icid.id_string());
	std::string cid_str = cid.to_string();

	// Check if the CID is local. If yes, queue job to push it to caller
	if(ctrl->is_CID_local(cid_str) == true) {
		// Schedule a job to have the chunk pushed to caller
		ICIDWorkRequestPtr work(
				std::make_unique<ICIDPushRequest>(cid.to_string(),
					src_dag.dag_string()));
		pool->queue_work(std::move(work));
		return;
	}
	// If not,
	// We need to fetch only if the chunk is not already in IRQ table
	if(irqtable->has_entry(cid_str)) {
		fetch_needed = true;
	}

	// add CID and requestor address to irq_table
	if(irqtable->add_fetch_request(cid_str, src_dag.dag_string()) == false) {
		std::cout << "Error adding fetch request to table" << std::endl;
		return;
	}

	// If we already sent an interest for this chunk, just wait for
	// it to be satisfied.
	// TODO: This prevents repeated requests from client. Is that OK?
	if(!fetch_needed) {
		return;
	}

	// Send a new ICID request with our socket's address
	sockaddr_x icid_addr;
	dst_dag.fill_sockaddr(&icid_addr);
	XinterestedInCID(recv_sock, &icid_addr);
}

void ICIDMonitor::handle_push_connection()
{
	int sock;
	sockaddr_x sa;
	socklen_t sa_len = sizeof(sa);

	// Accept an incoming connection for a pushed chunk
	if((sock = Xaccept(recv_sock, (sockaddr *)&sa, &sa_len)) < 0) {
		std::cout << "Error accepting connection from push req" << std::endl;
		return;
	}

	// Now create and queue up a job to handle the incoming chunk
	ICIDWorkRequestPtr work(std::make_unique<ICIDReceiveRequest>(sock));
	pool->queue_work(std::move(work));
	return;
}

void ICIDMonitor::monitor()
{
	fd_set rfds;
	fd_set all_rfds;

	// Add the ICID monitoring socket to all_rfds
	FD_SET(recv_sock, &all_rfds);
	FD_SET(icid_sock, &all_rfds);
	int max_rfd = recv_sock > icid_sock ? recv_sock: icid_sock;

	while(true) {

		// Prepare set of descriptors to monitor
		FD_ZERO(&rfds);
		rfds = all_rfds; // all_rfds is a single list for ICIDMonitor

		// Wait for at least one descriptor to be readable
		if(Xselect(max_rfd+1, &rfds, NULL, NULL, NULL) <= 0) {
			continue;
		}

		// Is the ICID descriptor ready to read?
		if(FD_ISSET(icid_sock, &rfds)) {
			handle_ICID_packet();
		}

		// Is there an incoming connection to push a chunk to us?
		if(FD_ISSET(recv_sock, &rfds)) {
			handle_push_connection();
		}
	}
}
