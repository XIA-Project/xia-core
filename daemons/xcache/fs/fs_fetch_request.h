#ifndef FS_FETCH_REQUEST_H
#define FS_FETCH_REQUEST_H

// Fetch Service includes
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

#define FS_FETCH_TIMEOUT 5
#define FS_FETCH_MAXBUF 2048

class FSFetchRequest : public FSWorkRequest {
	public:
		FSFetchRequest(InterestRequest &irq);
		static FSFetchRequest *from_client(std::string &buf);
		virtual ~FSFetchRequest();
		virtual void process();
	private:
		std::string chunk_id();

		std::string _chunk_addr;
		std::string _return_addr;
		std::string _signature;
		FSThreadPool *_pool;
		FSIRQTable *_irqtable;
		XcacheHandle _xcache;
};
#endif //FS_FETCH_REQUEST_H
