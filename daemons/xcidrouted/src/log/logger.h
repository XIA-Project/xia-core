#ifndef __LOGGER_H
#define __LOGGER_H

#include <fstream>
#include <ctime>

using namespace std;

class Logger{
public:
	Logger(const char* hostname);
	~Logger(); //Destructor

	void log(const char* data);
	void end();

private:
	ofstream logfile;
	time_t startTime;
};

#endif