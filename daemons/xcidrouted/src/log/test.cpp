#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds

#include "logger.h"

using namespace std;

int main(){
	Logger* logger = new Logger("test");

	this_thread::sleep_for(chrono::seconds(1));

	logger->log("test1");
	this_thread::sleep_for(chrono::seconds(2));

	logger->log("test2");
	this_thread::sleep_for(chrono::seconds(1));

	logger->log("test3");
	this_thread::sleep_for(chrono::seconds(1));

	logger->end();
	delete logger;

	return 0;
}