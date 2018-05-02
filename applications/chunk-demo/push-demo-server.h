#ifndef _PUSH_DEMO_SERVER_H
#define _PUSH_DEMO_SERVER_H

#include <string>
#include <iostream>

#include <Xsocket.h>
#include <xcache.h>
#include <dagaddr.hpp>
#include <Xkeys.h>	// XIA_SHA_DIGEST_STR_LEN

class PushDemoServer{
    public:
        PushDemoServer();
        ~PushDemoServer();
        int serve(sockaddr_x *remoteAddr);
	private:
		int createRandomChunk(sockaddr_x *addr);
		int _sockfd;
		XcacheHandle _xcache;
};

#endif // _PUSH_DEMO_SERVER_H
