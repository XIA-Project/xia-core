#ifndef _FETCH_DEMO_CLIENT_H
#define _FETCH_DEMO_CLIENT_H

#include <string>
#include <iostream>
#include <mutex>
#include <condition_variable>

#include <Xsocket.h>
#include <xcache.h>
#include <dagaddr.hpp>

class FetchDemoClient{
    public:
        FetchDemoClient();
        ~FetchDemoClient();
        int request(std::string &chunk_dag, std::string &fs_dag);
		void get_chunk(std::string &cid, std::string &data);
	private:
		XcacheHandle _xcache;
		static void gotChunkData(XcacheHandle *h, int event,
				void *data, size_t datalen);
};

#endif // _FETCH_DEMO_CLIENT_H
