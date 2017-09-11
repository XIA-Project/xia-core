#ifndef _NCID_DEMO_CLIENT_H
#define _NCID_DEMO_CLIENT_H

#include "ncid-demo-info.h"

#include <string>
#include <iostream>

#include <Xsocket.h>
#include <xcache.h>
#include <dagaddr.hpp>
#include <Xkeys.h>	// XIA_SHA_DIGEST_STR_LEN

class NCIDDemoClient : public NCIDDemoInfo{
    public:
        NCIDDemoClient();
        ~NCIDDemoClient();
        int fetch();
	private:
		int _sockfd;
		XcacheHandle _xcache;
};

#endif // _NCID_DEMO_CLIENT_H
