#include "icid_thread_pool.h"
#include "icid_work_request.h"

#include <thread>
#include <iostream>

// The only thread pool instance - initialized by first call to get_pool()
ICIDThreadPool* ICIDThreadPool::_instance = nullptr;

ICIDThreadPool::ICIDThreadPool()
{
	_instance = nullptr;

	// For now, number of threads depends on number of processors
	_num_threads = std::thread::hardware_concurrency();
	std::cout << "System concurrency: " << _num_threads << std::endl;
	if(_num_threads < 2) {
		_num_threads = 2;
	}
	std::cout << "Number of threads in pool: " << _num_threads << std::endl;

}

/*!
 * @brief get_pool() is the only way to get a reference to the thread_pool
 */
ICIDThreadPool* ICIDThreadPool::get_pool()
{
	if(_instance == nullptr) {
		_instance = new ICIDThreadPool;
	}
	return _instance;
}

/*!
 * @brief work() the worker thread
 *
 * Simply fetch the next callable work object off the work_queue
 * and execute the work
 */
void ICIDThreadPool::work()
{
	std::cout << std::this_thread::get_id() << ": Started" << std::endl;
	while(!_stop) {
		ICIDWorkRequestPtr work = fetch_work();
		work->process();
		// At this point the unique_ptr work goes out of scope
		// and ICIDWorkRequest destructor should get called
	}
}

/*!
 * @brief All work is queued into the ICIDWorkQueue
 */
int ICIDThreadPool::queue_work(ICIDWorkRequestPtr work)
{
	// Initialize the worker threads once
	std::call_once(_initialized, [&]() {
		for (unsigned int i=0; i< _num_threads; i++) {
			std::cout << "Creating thread: " << i << std::endl;
			/*
			ICIDWorker *worker = new ICIDWorker();
			std::thread work_thread(std::ref(*worker), i);
			_workers.push_back(worker);
			*/
			_worker_threads.push_back(std::thread(
						&ICIDThreadPool::work, this));
			/*
			_workers.emplace_back();
			_worker_threads.push_back(std::thread(_workers.back()));
			*/
		}
	});

	std::cout << "Queuing work" << std::endl;
	// TODO: Do we need to validate the 'work' request here?
	// Block others from accessing the work queue and push "work" to it
	{
		std::lock_guard<std::mutex> lock(_work_queue_mutex);
		_work_queue.push(std::move(work));
	}
	std::cout << "Work queued. Notifying a worker" << std::endl;
	_work_in_queue.notify_one();
	return 0;
}

/*!
 * @brief Fetch work from the ICIDWorkQueue in this ThreadPool
 *
 * This blocking function must be called by ICIDWorker threads
 * and it returns when the queue has some work for the worker
 * to complete.
 */
ICIDWorkRequestPtr ICIDThreadPool::fetch_work()
{

	// Block until work is available
	std::unique_lock<std::mutex> lock(_work_queue_mutex);

	// Unlock and wait until there is a request in the queue
	_work_in_queue.wait(lock, [&]{return _work_queue.size() > 0 ||
			_stop == true;});

	// _work_queue_mutex is locked; automaticall freed at end of function
	// Pull a request off the queue to process
	ICIDWorkRequestPtr request = std::move(_work_queue.front());
	_work_queue.pop();

	return request;
}
