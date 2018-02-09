#ifndef _FS_THREAD_POOL_H
#define _FS_THREAD_POOL_H

#include <condition_variable>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>

class FSWorker;
class FSWorkRequest;

class FSThreadPool {
	public:
		static FSThreadPool *get_pool(); // A reference to this thread pool

		int queue_work(FSWorkRequest *work); // Add work to work queue

		FSWorkRequest* fetch_work(); // Workers use this to pull work off queue

		// TODO:
		// fetch_work() should be accessible only to workers

	protected:
		FSThreadPool();
		~FSThreadPool();

	private:
		// There is only one thread pool instance
		static FSThreadPool *_instance;

		// Total number of threads in this pool - based on num of processors
		unsigned int _num_threads;

		// Worker objects and corresponding threads
		std::vector<FSWorker*> _workers;
		std::vector<std::thread> _worker_threads;

		// Work Queue, a mutex to protect it
		std::queue<FSWorkRequest*> _work_queue;
		std::mutex _work_queue_mutex;

		// Workers wait on condition that there is work in queue
		std::condition_variable _work_in_queue;

};
#endif //_FS_THREAD_POOL_H

