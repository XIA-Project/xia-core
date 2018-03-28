#ifndef FS_PUSH_REQUEST_H
#define FS_PUSH_REQUEST_H

// Push Service includes
#include "fs_work_request.h"
#include "fs_thread_pool.h"
#include "fs_irq_table.h"
#include "irq.pb.h"

// XIA includes
#include "xcache.h"

// System includes
#include <chrono>
#include <thread>
#include <iostream>	// TODO remove debug prints and this include

#define FS_PUSH_TIMEOUT 5
#define FS_PUSH_MAXBUF 2048

class FSPushRequest : public FSWorkRequest {
	public:
		FSPushRequest(std::string cid, std::string requestor);
		virtual ~FSPushRequest();
		virtual void process();
	private:
		std::string _cid;
		std::string _requestor;
		FSThreadPool *_pool;
		FSIRQTable *_irqtable;
		XcacheHandle _xcache;
};
#endif //FS_PUSH_REQUEST_H
