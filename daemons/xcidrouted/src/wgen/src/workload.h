#ifndef _WORKLOAD_H
#define _WORKLOAD_H

#include <algorithm>
#include <random>
#include <chrono>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include <map>

#include "config.h"
#include "../../common/struct.h"

using namespace std;

class Workload{
public:
	virtual void save(const char* templateFileName) = 0;
private:
	virtual void generateChunkWorkload(const ConfigParser* parser) = 0;
	virtual void generateFileWorkload(const ConfigParser* parser) = 0;
	virtual void generateVideoWorkload(const ConfigParser* parser) = 0;
};

class ClientWorkload: Workload{
public:
	ClientWorkload(const ConfigParser* parser, map<int, int> obj2Chunk);
	~ClientWorkload();

	void save(const char* templateFileName) override;
	void printClientRequestsAssignment() const;
private:
	void generateChunkWorkload(const ConfigParser* parser) override;
	void generateFileWorkload(const ConfigParser* parser) override;
	void generateVideoWorkload(const ConfigParser* parser) override;

	map<int, int> obj2Chunk;
	vector<vector<RequestProperty>> clientRequestsAssignment;
};

class ServerWorkload: Workload{
public:
	ServerWorkload(const ConfigParser* parser);
	~ServerWorkload();

	void save(const char* templateFileName) override;
	void printServerContentAssignment() const;
	map<int, int> getObj2Chunk() const;
private:
	void generateChunkWorkload(const ConfigParser* parser) override;
	void generateFileWorkload(const ConfigParser* parser) override;
	void generateVideoWorkload(const ConfigParser* parser) override;

	map<int, int> obj2Chunk;
	// index 0 (server 0) -> list of files
	vector<vector<ContentProperty>> serverContentAssignment;
};

#endif