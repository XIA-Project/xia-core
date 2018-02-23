// Project includes
#include "fs_worker.h"
#include "fs_thread_pool.h"
#include "fs_sleep_request.h"
#include "fs_client_request.h"

// XIA includes
#include "dagaddr.hpp"
#include "Xkeys.h"
#include "Xsocket.h"

// Standard library includes
#include <vector>
#include <thread>
#include <chrono>
#include <iostream>

// The Chunk Fetching Service
//
// Fetches requested chunks from a remote or local location and pushes
// the fetched chunks to the requesting services on <potentially mobile>
// clients.
int main()
{
	int sock;			// Xsocket to listen on
	int state = 0;		// cleanup state
	int retval = -1;	// Return error by default
	Graph *g;

	char sid_string[XIA_XID_STR_SIZE];
	int sid_strlen = sizeof(sid_string);

	sockaddr_x *sa;
	struct addrinfo hints, *ai;
	bzero(&hints, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_XIA;

	// A pool of threads to process FS requests and responses
	FSThreadPool *pool = FSThreadPool::get_pool();

	// A new SID for the Fetching Service to listen on
	if(XmakeNewSID(sid_string, sid_strlen)) {
		std::cout << "Error making new SID for Fetching Service" << std::endl;
		goto fs_done;
	}
	state = 1; // cleanup the SID we created

	// Create an XSocket
	if((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		std::cout << "Error creating socket to listen on" << std::endl;
		goto fs_done;
	}

	// Our address that clients will send requests on
	if(Xgetaddrinfo(NULL, sid_string, &hints, &ai) != 0) {
		std::cout << "Error finding our address" << std::endl;
		goto fs_done;
	}
	state = 2;
	sa = (sockaddr_x *)ai->ai_addr;

	// Wait forever for incoming requests from clients
	if(Xbind(sock, (struct sockaddr *)sa, sizeof(sockaddr_x)) < 0) {
		std::cout << "Error binding to our address" << std::endl;
		goto fs_done;
	}
	state = 3;
	g = new Graph(sa);
	std::cout << "FetchingService address: " << g->dag_string() << std::endl;

	if(Xlisten(sock, 5)) {
		std::cout << "Error listening on our address" << std::endl;
		goto fs_done;
	}

	while(true) {

		int t_sock;
		sockaddr_x their_addr;
		socklen_t their_addrlen = sizeof(their_addr);

		std::cout << "Waiting for a client connection" << std::endl;
		t_sock = Xaccept(sock, (sockaddr *)&their_addr, &their_addrlen);
		if(t_sock < 0) {
			std::cout << "Failed accepting client connection" << std::endl;
			continue;
		}
		Graph g(&their_addr);
		std::cout << "Connected to " << g.dag_string() << std::endl;

		// A thread in the pool will now interact with client
		FSWorkRequest *work = new FSClientRequest(t_sock);
		pool->queue_work(work);
	}
	// TODO: Build a way to break out of loop when user requests
	// that will be considered a successful exit.
	retval = 0;

	// Validate the request
	// Create a FSWorkRequest object and queue it for processing
	//

	/*
	// Test - simply queue up dummy sleep requests:
	for(int i=0; i<10; i++) {
		FSWorkRequest *work = new FSSleepRequest(2);
		pool->queue_work(work);
	}
	std::this_thread::sleep_for(std::chrono::seconds(50));
	*/
	/*
	FSWorker *worker;

	// Create one worker
	worker = new FSWorker(1);
	std::thread worker_thread(*worker, 234);
	worker_thread.join();

	// Now create a hundred
	std::cout << "Creating 100 threads" << std::endl;
	unsigned int num_threads = 100;
	std::vector<std::thread> threads;
	std::vector<FSWorker*> workers;
	for(unsigned int i=0;i<num_threads;i++) {
		worker = new FSWorker(i);
		workers.push_back(worker);
		threads.push_back(std::thread(*worker, i));
	}
	// Wait for them all to complete
	for(unsigned int i=0;i<threads.size();i++) {
		threads[i].join();
	}
	for(unsigned int i=0;i<workers.size();i++) {
		delete workers[i];
	}
	threads.clear();

	std::cout << "100 Threads completed executing" << std::endl;
	*/
fs_done:
	switch(state) {
		case 3:
			Xclose(sock);
		case 2:
			Xfreeaddrinfo(ai);
		case 1:
			// TODO: Can check if XexistsSID before attempting remove
			XremoveSID(sid_string);
	}
	return retval;
}
