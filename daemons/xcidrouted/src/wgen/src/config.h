#ifndef _CONFIG_H
#define _CONFIG_H

#include <vector>
#include <string>

using namespace std;

// character separator
extern const char COMMENT;
extern const char* DELIMITER;

// general configuration strings
extern const char* CHUNK_SIZE;
extern const char* NUM_CLIENTS;
extern const char* NUM_SERVERS;
extern const char* RATE_REQUEST;
extern const char* NUM_REQUESTS;
extern const char* ZIPF_ALPHA;

// workload type definitions
extern const char* WORKLOAD_TYPE;
extern const char* WORKLOAD_TYPE_CHUNK;
extern const char* WORKLOAD_TYPE_FILE;
extern const char* WORKLOAD_TYPE_VIDEO;

// chunk configuration strings
extern const char* NUM_CHUNKS;

// file configuration strings
extern const char* NUM_FILES;
extern const char* MIN_FILE_NUM_CHUNKS;
extern const char* MAX_FILE_NUM_CHUNKS;

// video configuration strings
extern const char* NUM_VIDEOS;
extern const char* MIN_VIDEO_NUM_CHUNKS;
extern const char* MAX_VIDEO_NUM_CHUNKS;
extern const char* VIDEO_CHUNK_ZIPF_ALPHA;

class ConfigParser{
public:
	ConfigParser();
	~ConfigParser(); //Destructor

	void parse(const char* configFileName);

	// general configuration for all workload types
	int chunkSize = -1;
	int numClients = -1;
	int numServers = -1;
	double rateRequests = -1;
	int numRequests = -1;
	double zipfAlpha = -1;

	// workload specific configuration
	string workloadType = "";

	// chunk workload type configurations
	int numChunks = -1;

	// file workload type configurations
	int numFiles = -1;
	int minFileNumChunks = -1;
	int maxFileNumChunks = -1;

	// video workload type configurations
	int numVideos = -1;
	int minVideoNumChunks = -1;
	int maxVideoNumChunks = -1;
	double videoChunkZipfAlpha = -1;

private:
	vector<string> splitStringDelimiter(char* str, const char* delimiter);
};

#endif