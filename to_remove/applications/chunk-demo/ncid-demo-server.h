#ifndef _NCID_DEMO_SERVER_H
#define _NCID_DEMO_SERVER_H

#include "ncid-demo-info.h"

#include <string>
#include <iostream>

#include <Xsocket.h>
#include <xcache.h>
#include <dagaddr.hpp>
#include <Xkeys.h>	// XIA_SHA_DIGEST_STR_LEN

class NCIDDemoServer : public NCIDDemoInfo{
    public:
        NCIDDemoServer();
        ~NCIDDemoServer();
        int serve();
	private:
		int _sockfd;
		XcacheHandle _xcache;
};

#endif // _NCID_DEMO_SERVER_H
