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
	int _sockfd;
	sockaddr_x _sa;

	static void *run(void *);
	using RfdList = std::vector<int>;

	int wait_for_read(RfdList &rfdlist);
	const static int _timeoutsec = 1;
	const static int _timeoutusec = 0;
	bool _stopping;
public:
	/**
	 * A Constructor.
	 */
	PushProxy();
	void operator() (xcache_controller *, int context_ID);
	void stop();
	~PushProxy();
	std::string addr();
};


#endif	// __PUSHPROXY_H__
