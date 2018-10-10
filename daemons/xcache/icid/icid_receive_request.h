#ifndef ICID_RECEIVE_REQUEST_H
#define ICID_RECEIVE_REQUEST_H

// Fetch Service includes
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

#define ICID_RECEIVE_TIMEOUT 5
#define ICID_RECEIVE_MAXBUF 2048

class ICIDReceiveRequest : public ICIDWorkRequest {
	public:
		ICIDReceiveRequest(int sock);
		static ICIDReceiveRequest *from_client(std::string &buf);
		virtual ~ICIDReceiveRequest();
		virtual void process();
	private:
		void pushChunkTo(std::string cid, std::string requestor);

		ICIDThreadPool *_pool; // Thread pool to serve
		ICIDIRQTable *_irqtable; // Table of interest requests

		int sock; // Accepted socket to receive chunk on
};
#endif //ICID_RECEIVE_REQUEST_H
