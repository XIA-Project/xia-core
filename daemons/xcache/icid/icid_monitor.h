#ifndef _ICID_MONITOR_H_
#define _ICID_MONITOR_H_

#include "icid_thread_pool.h"
#include "icid_irq_table.h"
#include "controller.h"
#include "Xsocket.h"

class ICIDMonitor {
	public:
		void monitor();

	protected:
		ICIDMonitor();
		~ICIDMonitor();

	private:
		// Private functions to handle incoming requests
		void handle_ICID_packet();
		void handle_push_connection();

		// A temporary SID for receiving pushed chunks
		char sid_string[XIA_XID_STR_SIZE];
		bool sid_string_initialized;

		// A pool of threads to handle blocking requests
		ICIDThreadPool *pool;

		// UNIX domain socket to receive ICID packets
		int icid_sock;

		// Xsocket to receive pushed chunks
		int recv_sock;
		sockaddr_x recv_addr;

		// Reference to controller, for local CID checks and pushes
		xcache_controller *ctrl;

		// Reference to the Interest Request table
		ICIDIRQTable *irqtable;
};
#endif // _ICID_MONITOR_H_
