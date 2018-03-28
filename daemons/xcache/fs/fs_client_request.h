#ifndef FS_CLIENT_REQUEST_H
#define FS_CLIENT_REQUEST_H

#include "fs_work_request.h"
#include "fs_thread_pool.h"

#include <chrono>
#include <thread>
#include <iostream>	// TODO remove debug prints and this include

#define FS_CLIENT_TIMEOUT 5
#define FS_CLIENT_MAXBUF 2048

class FSClientRequest : public FSWorkRequest {
	public:
		FSClientRequest(int sockfd);
		virtual ~FSClientRequest();
		virtual void process();
	private:
		int _sockfd;
		FSThreadPool *_pool;
};
#endif //FS_CLIENT_REQUEST_H
