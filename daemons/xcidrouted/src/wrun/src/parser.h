#ifndef _PARSER_H
#define _PARSER_H

#include <string>
#include <vector>
#include "../../common/struct.h"

using namespace std;

class WorkloadParser{
public:
	virtual ~WorkloadParser();
	virtual void parse(const char* workloadFileName) = 0;
	// helper functions useful for subclass
	static vector<string> splitStringDelimiter(char* str, const char* delimiter);
};

class ClientWorkloadParser: WorkloadParser{
public:
	ClientWorkloadParser();
	~ClientWorkloadParser();

	void parse(const char* workloadFileName) override;
	void printRequests();

	vector<RequestProperty> requests;	
};

class ServerWorkloadParser: WorkloadParser{
public:
	ServerWorkloadParser();
	~ServerWorkloadParser();

	void parse(const char* workloadFileName) override;
	void printChunks();

	vector<ContentProperty> chunks;
};

#endif