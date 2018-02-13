#include "fs_worker.h"
#include "fs_work_request.h"
#include "fs_thread_pool.h"

#include <thread>
#include <iostream>

FSWorker::FSWorker()
{
	_pool = FSThreadPool::get_pool();
}

void FSWorker::operator() (unsigned int id) {
	_id = id;
	std::cout << std::this_thread::get_id() <<
		": Working" << std::endl;
	std::cout << "ID: " << _id << std::endl;

	while (true) {

		// Get work from the thread pool work queue
		FSWorkRequest *work = _pool->fetch_work();

		// Schedule work
		work->process();

		// Now destroy the work object and free its resources
		delete work;
	}
}
