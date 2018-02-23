#ifndef _FETCH_DEMO_CLIENT_H
#define _FETCH_DEMO_CLIENT_H

#include <string>
#include <iostream>

#include <Xsocket.h>
#include <xcache.h>
#include <dagaddr.hpp>

class FetchDemoClient{
    public:
        FetchDemoClient();
        ~FetchDemoClient();
        int request(std::string &chunk_dag, std::string &fs_dag);
	private:
		XcacheHandle _xcache;
};

#endif // _FETCH_DEMO_CLIENT_H
