#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <iostream>
#include "parser.h"

WorkloadParser::~WorkloadParser(){};

vector<string> WorkloadParser::splitStringDelimiter(char* str, const char* delimiter){
	vector<string> result;

    char * pch;
    pch = strtok (str, delimiter);
    while (pch != NULL) {
        result.push_back(pch);
        pch = strtok (NULL, delimiter);
    }

    return result;
}

ClientWorkloadParser::ClientWorkloadParser(){};
ClientWorkloadParser::~ClientWorkloadParser(){};

void ClientWorkloadParser::parse(const char* workloadFileName){
	string line;
	ifstream infile(workloadFileName);

	while (getline(infile, line)) {
		if(line == ""){
			continue;
		}

		vector<string> parsedLine = WorkloadParser::splitStringDelimiter((char*)line.c_str(), " ");
		RequestProperty request = {atoi(parsedLine[0].c_str()), atoi(parsedLine[1].c_str()), 
							atoi(parsedLine[2].c_str()), atof(parsedLine[3].c_str())};

		this->requests.push_back(request);
	}

}

void ClientWorkloadParser::printRequests(){
	cout << "parsed requests: \n";
	for(unsigned i = 0; i < this->requests.size(); i++){
		cout << "\t object id: " << this->requests[i].objectId << " chunk id: " << this->requests[i].chunkId << " chunk size: " << this->requests[i].chunkSize 
		<< " delay: " << this->requests[i].delay << endl;
	}
}

ServerWorkloadParser::ServerWorkloadParser(){};
ServerWorkloadParser::~ServerWorkloadParser(){};

void ServerWorkloadParser::parse(const char* workloadFileName){
	string line;
	ifstream infile(workloadFileName);

	while (getline(infile, line)) {
		if(line == ""){
			continue;
		}

		vector<string> parsedLine = splitStringDelimiter((char*)line.c_str(), " ");
		ContentProperty chunk = {atoi(parsedLine[0].c_str()), atoi(parsedLine[1].c_str()), atoi(parsedLine[2].c_str())};

		this->chunks.push_back(chunk);
	}
}

void ServerWorkloadParser::printChunks(){
	cout << "parsed chunks: \n";
	for(unsigned i = 0; i < this->chunks.size(); i++){
		cout << "\t object id: " << this->chunks[i].objectId << " chunk id: " << this->chunks[i].chunkId << " chunk size: " << this->chunks[i].chunkSize << endl;
	}
}