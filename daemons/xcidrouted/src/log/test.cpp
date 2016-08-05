#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds

#include <signal.h>
#include "logger.h"

using namespace std;

static Logger* logger;

void cleanup(int){
	logger->end();
	delete logger;

	exit(1);
}

int main(){
	(void) signal(SIGINT, cleanup);

	logger = new Logger("test");

	logger->log("test1");

	logger->log("test2");

	logger->log("test3");

	while(1){};

	return 0;
}