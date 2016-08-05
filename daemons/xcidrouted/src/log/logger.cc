#include "logger.h"

Logger::Logger(const char* hostname){
	this->startTime = time(0);

	string logFileName("log/");
	logFileName += hostname;
	logFileName += "_";
	logFileName += ctime(&startTime);
	logFileName.pop_back();
	logFileName += ".dat";

	this->logfile.open(logFileName.c_str());
	// print out the start time
	this->logfile << 0 << endl;
};

Logger::~Logger(){
	if(this->logfile.is_open()){
		this->logfile.close();
	}
};

void Logger::log(const char* data){
	time_t currTime = time(0);
	double sinceThen = difftime(currTime, this->startTime);

	this->logfile << sinceThen << " " << data << endl;
	this->logfile.flush();
}

void Logger::end(){
	time_t currTime = time(0);
	double sinceThen = difftime(currTime, this->startTime);
	this->logfile << sinceThen << endl;
}
