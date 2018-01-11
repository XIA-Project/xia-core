#ifndef __PUSHPROXY_H__
#define __PUSHPROXY_H__

#include <Xsocket.h>
#include <dagaddr.hpp>
#include <thread>
#include <iostream>
#include "controller.h"

/**
 * A proxy to handle pushed chunks
 */

class PushProxy {
private:
	size_t _sid_strlen;
	char *_sid_string;
	Graph *_proxy_addr;

	static void *run(void *);
public:
	/**
	 * A Constructor.
	 */
	PushProxy();
	void operator() (xcache_controller *);
	~PushProxy();
	std::string addr();
};


#endif	// __PUSHPROXY_H__
