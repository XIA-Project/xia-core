#ifndef _FS_WORKER_H
#define _FS_WORKER_H

#include <thread>
#include <iostream>

class FSThreadPool;

class FSWorker {

	public:
		FSWorker();

		void operator() (unsigned int id);

	private:
		unsigned int _id;
		FSThreadPool *_pool; // The thread pool this worker belongs to

};
#endif //_FS_WORKER_H
