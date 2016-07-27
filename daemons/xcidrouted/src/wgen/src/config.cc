#include <sstream>
#include <fstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include "config.h"

// character separator
const char COMMENT = '#';
const char* DELIMITER = "=";

// general configuration strings
const char* CHUNK_SIZE="CHUNK_SIZE";
const char* NUM_CLIENTS="NUM_CLIENTS";
const char* NUM_SERVERS="NUM_SERVERS";
const char* RATE_REQUEST="RATE_REQUEST";
const char* NUM_REQUESTS="NUM_REQUESTS";
const char* ZIPF_ALPHA="ZIPF_ALPHA";

// workload type definitions
const char* WORKLOAD_TYPE="WORKLOAD_TYPE";
const char* WORKLOAD_TYPE_CHUNK="CHUNK";
const char* WORKLOAD_TYPE_FILE="FILE";
const char* WORKLOAD_TYPE_VIDEO="VIDEO";

// chunk configuration strings
const char* NUM_CHUNKS="NUM_CHUNKS";

// file configuration strings
const char* NUM_FILES="NUM_FILES";
const char* MIN_FILE_NUM_CHUNKS="MIN_FILE_NUM_CHUNKS";
const char* MAX_FILE_NUM_CHUNKS="MAX_FILE_NUM_CHUNKS";

// video configuration strings
const char* NUM_VIDEOS="NUM_VIDEOS";
const char* MIN_VIDEO_NUM_CHUNKS="MIN_VIDEO_NUM_CHUNKS";
const char* MAX_VIDEO_NUM_CHUNKS="MAX_VIDEO_NUM_CHUNKS";
const char* VIDEO_CHUNK_ZIPF_ALPHA="VIDEO_CHUNK_ZIPF_ALPHA";

ConfigParser::ConfigParser(){};
ConfigParser::~ConfigParser(){};

void ConfigParser::parse(const char* configFileName){
	string line;
	ifstream infile(configFileName);

	while (getline(infile, line)) {
    	if(line[0] == COMMENT || line == ""){
    		continue;
    	}

    	vector<string> parsedLine = splitStringDelimiter((char*)line.c_str(), DELIMITER);
    	string type = parsedLine[0];
        string value = parsedLine[1];

        if(type.compare(CHUNK_SIZE) == 0){
            this->chunkSize = atoi(value.c_str());
        } else if (type.compare(NUM_CLIENTS) == 0){
            this->numClients = atoi(value.c_str());
        } else if (type.compare(NUM_SERVERS) == 0){
            this->numServers = atoi(value.c_str());
        } else if (type.compare(RATE_REQUEST) == 0){
            this->rateRequests = atof(value.c_str());
        } else if (type.compare(NUM_REQUESTS) == 0){
            this->numRequests = atoi(value.c_str());
        } else if (type.compare(ZIPF_ALPHA) == 0){
            this->zipfAlpha = atof(value.c_str());
        } else if (type.compare(WORKLOAD_TYPE) == 0){
            this->workloadType = value;
        } else if (type.compare(NUM_CHUNKS) == 0){
            this->numChunks = atoi(value.c_str());
        } else if (type.compare(NUM_FILES) == 0){
            this->numFiles = atoi(value.c_str());
        } else if (type.compare(MIN_FILE_NUM_CHUNKS) == 0){
            this->minFileNumChunks = atoi(value.c_str());
        } else if (type.compare(MAX_FILE_NUM_CHUNKS) == 0){
            this->maxFileNumChunks = atoi(value.c_str());
        } else if (type.compare(NUM_VIDEOS) == 0){
            this->numVideos = atoi(value.c_str());
        } else if (type.compare(MIN_VIDEO_NUM_CHUNKS) == 0){
            this->minVideoNumChunks = atoi(value.c_str());
        } else if (type.compare(MAX_VIDEO_NUM_CHUNKS) == 0){
            this->maxVideoNumChunks = atoi(value.c_str());
        } else if (type.compare(VIDEO_CHUNK_ZIPF_ALPHA) == 0){
            this->videoChunkZipfAlpha = atof(value.c_str());
        }
    }
}

vector<string> ConfigParser::splitStringDelimiter(char* str, const char* delimiter){
    vector<string> result;

    char * pch;
    pch = strtok (str, delimiter);
    while (pch != NULL) {
        result.push_back(pch);
        pch = strtok (NULL, delimiter);
    }

    return result;
}