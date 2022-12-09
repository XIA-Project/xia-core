#ifndef _PUSH_DEMO_CLIENT_H
#define _PUSH_DEMO_CLIENT_H

#include <string>
#include <iostream>

#include <Xsocket.h>
#include <xcache.h>
#include <dagaddr.hpp>
#include <Xkeys.h>	// XIA_SHA_DIGEST_STR_LEN

class PushDemoClient{
    public:
        PushDemoClient();
        ~PushDemoClient();
        int fetch();
	private:
		int _sockfd;
		size_t _sid_strlen;
		char *_sid_string;
		XcacheHandle _xcache;
};

#endif // _PUSH_DEMO_CLIENT_H
