#ifndef ICID_PUSH_REQUEST_H
#define ICID_PUSH_REQUEST_H

// Push Service includes
#include "icid_work_request.h"
#include "icid_thread_pool.h"
#include "icid_irq_table.h"
#include "irq.pb.h"

// XIA includes
#include "xcache.h"

// System includes
#include <chrono>
#include <thread>
#include <iostream>	// TODO remove debug prints and this include

#define ICID_PUSH_TIMEOUT 5
#define ICID_PUSH_MAXBUF 2048

class ICIDPushRequest : public ICIDWorkRequest {
	public:
		ICIDPushRequest(std::string cid, std::string requestor);
		virtual ~ICIDPushRequest();
		virtual void process();
	private:
		std::string _cid;
		std::string _requestor;
		ICIDThreadPool *_pool;
		ICIDIRQTable *_irqtable;
		XcacheHandle _xcache;
};
#endif //ICID_PUSH_REQUEST_H
