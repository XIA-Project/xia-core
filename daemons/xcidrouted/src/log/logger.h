#ifndef __LOGGER_H
#define __LOGGER_H

#include <fstream>
#include <string>
#include <ctime>

using namespace std;

class Logger{
public:
	Logger(const char* hostname);
	Logger(const string name);
	~Logger(); //Destructor

	void log(const char* data);
	void log(string data);
	void end();

private:
	ofstream logfile;
	time_t startTime;
};

#endif