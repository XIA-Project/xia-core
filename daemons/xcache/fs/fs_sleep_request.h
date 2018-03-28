#ifndef FS_SLEEP_REQUEST_H
#define FS_SLEEP_REQUEST_H

#include "fs_work_request.h"

#include <chrono>
#include <thread>
#include <iostream>	// TODO remove debug prints and this include

class FSSleepRequest : public FSWorkRequest {
	public:
		FSSleepRequest(unsigned int seconds) {_seconds = seconds;}
		virtual ~FSSleepRequest() {}
		virtual void process() {
			auto thread_id = std::this_thread::get_id();

			std::cout << thread_id <<
				": Sleep Request thread sleeping for " <<
				_seconds << " seconds" << std::endl;

			std::this_thread::sleep_for(std::chrono::seconds(_seconds));

			std::cout << thread_id <<
				": Sleep Request thread awake" << std::endl;
		}
	private:
		int _seconds;
};
#endif //FS_SLEEP_REQUEST_H
