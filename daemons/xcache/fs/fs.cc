// Project includes
#include "fs_worker.h"
#include "fs_thread_pool.h"
#include "fs_sleep_request.h"

// Standard library includes
#include <vector>
#include <thread>
#include <chrono>

// The Chunk Fetching Service
//
// Fetches requested chunks from a remote or local location and pushes
// the fetched chunks to the requesting services on <potentially mobile>
// clients.
int main()
{
	// Basic algorithm:
	// ---------------
	// Create an XSocket
	// Wait for incoming requests from clients
	// Validate the request
	// Create a FSWorkRequest object and queue it for processing
	//

	// Test - simply create a pool and queue up dummy sleep requests:
	FSThreadPool *pool = FSThreadPool::get_pool();
	for(int i=0; i<10; i++) {
		FSWorkRequest *work = new FSSleepRequest(2);
		pool->queue_work(work);
	}
	std::this_thread::sleep_for(std::chrono::seconds(50));
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
	return 0;
}
