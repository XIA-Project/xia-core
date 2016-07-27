#include "workload.h"

using namespace std;

ClientWorkload::ClientWorkload(const ConfigParser* parser, map<int, int> obj2Chunk){
	this->obj2Chunk = obj2Chunk;

	if(parser->workloadType.compare(WORKLOAD_TYPE_CHUNK) == 0){
		generateChunkWorkload(parser);
	} else if (parser->workloadType.compare(WORKLOAD_TYPE_FILE) == 0){
		generateFileWorkload(parser);
	} else if (parser->workloadType.compare(WORKLOAD_TYPE_VIDEO) == 0){
		generateVideoWorkload(parser);
	} else {
		printf("Unknown workload\n");
	}
};

ClientWorkload::~ClientWorkload(){};

void ClientWorkload::generateChunkWorkload(const ConfigParser* parser){
	// reset the chunk rank accouting
	this->clientRequestsAssignment.clear();

	// push all chunk ids first
	double zipfDenominator = 0.0;
	for (int i = 0; i < parser->numChunks; ++i) {
		zipfDenominator += (1/pow(i+1, parser->zipfAlpha));
	}

	vector<int> allChunkRequests;
	// assign request frequencies to the ranked chunk id based on Zipf law
	// https://en.wikipedia.org/wiki/Zipf%27s_law
	// then produce the full request lists
	for (int i = 0; i < parser->numChunks; ++i) {
		double currFrequency = (1/pow(i+1, parser->zipfAlpha)) / zipfDenominator;
		long currRepeatedRequests = round(currFrequency * parser->numRequests);

		for(int j = 0; j < currRepeatedRequests; j++){
			allChunkRequests.push_back(i);
		}
	}

	// randomly shuffle all the chunks
	srand((unsigned)time(NULL));
	std::random_shuffle(allChunkRequests.begin(), allChunkRequests.end());

	// initialize the accounting data structure with another vector.
	for(int i = 0; i < parser->numClients; i++){
		vector<RequestProperty> requestProperties;
		this->clientRequestsAssignment.push_back(requestProperties);
	}

	// assign chunk to clients
	int rrPointer = 0;
	int totalNumChunks = (int)allChunkRequests.size();
	int partitionSize = totalNumChunks/(1.0*parser->numClients);
	for(int i = 0; i < totalNumChunks; i++){
		int currClient = i / partitionSize;
		if(currClient >= this->clientRequestsAssignment.size()){
			currClient = (rrPointer++)%parser->numClients;
		}

		// exponential delay for the client requests
		double currRand = (double)rand()/(double)RAND_MAX;
		double nextTime = -1*log(currRand)/parser->rateRequests;	// this is in seconds
		RequestProperty currRequestProperty = {-1, allChunkRequests[i], parser->chunkSize, nextTime*1000};
		this->clientRequestsAssignment[currClient].push_back(currRequestProperty);
	}
}

void ClientWorkload::generateFileWorkload(const ConfigParser* parser){
	// reset the chunk rank accouting
	this->clientRequestsAssignment.clear();

	// push all file ids first
	double zipfDenominator = 0.0;
	for (int i = 0; i < parser->numFiles; ++i) {
		zipfDenominator += (1/pow(i+1, parser->zipfAlpha));
	}

	vector<int> allFileRequests;
	// assign request frequencies to the ranked file id based on Zipf law
	// https://en.wikipedia.org/wiki/Zipf%27s_law
	// then produce the full request lists
	for (int i = 0; i < parser->numFiles; ++i) {
		double currFrequency = (1/pow(i+1, parser->zipfAlpha)) / zipfDenominator;
		long currRepeatedRequests = round(currFrequency * parser->numRequests);

		for(int j = 0; j < currRepeatedRequests; j++){
			allFileRequests.push_back(i);
		}
	}

	// randomly shuffle all the chunks
	srand((unsigned)time(NULL));
	std::random_shuffle(allFileRequests.begin(), allFileRequests.end());

	// initialize the accounting data structure with another vector.
	for(int i = 0; i < parser->numClients; i++){
		vector<RequestProperty> requestProperties;
		this->clientRequestsAssignment.push_back(requestProperties);
	}

	// assign chunk to clients
	int rrPointer = 0;
	int totalNumFiles = (int)allFileRequests.size();
	int partitionSize = totalNumFiles/(1.0*parser->numClients);
	for(int i = 0; i < totalNumFiles; i++){
		int currClient = i / partitionSize;
		if(currClient >= this->clientRequestsAssignment.size()){
			currClient = (rrPointer++)%parser->numClients;
		}
		int currFileId = allFileRequests[i];
		int maxChunkId = this->obj2Chunk[currFileId];
		// put the chunks and shuffle the request order
		vector<int> currChunks;
		for(int k = 0; k <= maxChunkId; k++){
			currChunks.push_back(k);
		}
		std::random_shuffle(currChunks.begin(), currChunks.end());

		for(int k = 0; k <= maxChunkId; k++){
			int currChunk = currChunks[k];
			// exponential delay for the client requests
			double currRand = (double)rand()/(double)RAND_MAX;
			double nextTime = -1*log(currRand)/parser->rateRequests;	// this is in seconds
			RequestProperty currRequestProperty = {currFileId, currChunk, parser->chunkSize, nextTime*1000};
			this->clientRequestsAssignment[currClient].push_back(currRequestProperty);
		}
	}
}

void ClientWorkload::generateVideoWorkload(const ConfigParser* parser){
	// reset the chunk rank accouting
	this->clientRequestsAssignment.clear();

	// push all file ids first
	double zipfDenominator = 0.0;
	for (int i = 0; i < parser->numVideos; ++i) {
		zipfDenominator += (1/pow(i+1, parser->zipfAlpha));
	}

	vector<int> allVideoRequests;
	// assign request frequencies to the ranked file id based on Zipf law
	// https://en.wikipedia.org/wiki/Zipf%27s_law
	// then produce the full request lists
	for (int i = 0; i < parser->numVideos; ++i) {
		double currFrequency = (1/pow(i+1, parser->zipfAlpha)) / zipfDenominator;
		long currRepeatedRequests = round(currFrequency * parser->numRequests);

		for(int j = 0; j < currRepeatedRequests; j++){
			allVideoRequests.push_back(i);
		}
	}

	// randomly shuffle all the chunks
	srand((unsigned)time(NULL));
	std::random_shuffle(allVideoRequests.begin(), allVideoRequests.end());

	// initialize the accounting data structure with another vector.
	for(int i = 0; i < parser->numClients; i++){
		vector<RequestProperty> requestProperties;
		this->clientRequestsAssignment.push_back(requestProperties);
	}

	// assign chunk to clients
	int rrPointer = 0;
	int totalNumVideos = (int)allVideoRequests.size();
	int partitionSize = totalNumVideos/(1.0*parser->numClients);
	for(int i = 0; i < totalNumVideos; i++){
		int currClient = i / partitionSize;
		if(currClient >= this->clientRequestsAssignment.size()){
			currClient = (rrPointer++)%parser->numClients;
		}
		int currVideoId = allVideoRequests[i];
		int maxChunkId = this->obj2Chunk[currVideoId];

		double zipfDenominator = 0.0;
		for(int k = 0; k <= maxChunkId; k++){
			zipfDenominator += (1/pow(k+1, parser->videoChunkZipfAlpha));
		}

		for(int k = 0; k <= maxChunkId; k++){
			double currFrequency = (1/pow(k+1, parser->videoChunkZipfAlpha)) / zipfDenominator;
			double currRand = (double)rand()/(double)RAND_MAX;
			if(currRand > currFrequency){
				break;
			} 

			double nextTime = -1*log(currRand)/parser->rateRequests;	// this is in seconds
			RequestProperty currRequestProperty = {currVideoId, k, parser->chunkSize, nextTime*1000};
			this->clientRequestsAssignment[currClient].push_back(currRequestProperty);
		}
	}
}

void ClientWorkload::printClientRequestsAssignment() const{
	for(int i = 0; i < this->clientRequestsAssignment.size(); i++){
		cout << "Client " << i << " sends" << endl;

		for(int j = 0; j < this->clientRequestsAssignment[i].size(); j++){
			cout << "\tobject id: " << this->clientRequestsAssignment[i][j].objectId << " chunk id: " << this->clientRequestsAssignment[i][j].chunkId << 
				" chunk size: " << this->clientRequestsAssignment[i][j].chunkSize << " delay: " << this->clientRequestsAssignment[i][j].delay << endl;
		}
	}
}

void ClientWorkload::save(const char* templateFileName){
	for(int i = 0; i < this->clientRequestsAssignment.size(); i++){
		ostringstream ss;
		ss << templateFileName << i+1 << ".dat";
		string currFileName = ss.str();
		
		ofstream currFile;
		currFile.open(currFileName);

		for(int j = 0; j < this->clientRequestsAssignment[i].size(); j++){
			currFile << this->clientRequestsAssignment[i][j].objectId << " "
				<< this->clientRequestsAssignment[i][j].chunkId << " "
				<< this->clientRequestsAssignment[i][j].chunkSize << " " 
				<< this->clientRequestsAssignment[i][j].delay << endl;
		}
		currFile.close();
	}
}

ServerWorkload::ServerWorkload(const ConfigParser* parser){
	if(parser->workloadType.compare(WORKLOAD_TYPE_CHUNK) == 0){
		generateChunkWorkload(parser);
	} else if (parser->workloadType.compare(WORKLOAD_TYPE_FILE) == 0){
		generateFileWorkload(parser);
	} else if (parser->workloadType.compare(WORKLOAD_TYPE_VIDEO) == 0){
		generateVideoWorkload(parser);
	} else {
		printf("Unknown workload\n");
	}
};

ServerWorkload::~ServerWorkload(){};

void ServerWorkload::generateChunkWorkload(const ConfigParser* parser){
	// reset the server chunk assignment accounting
	this->serverContentAssignment.clear();

	// push all the chunk ids first
	vector<int> allChunkIDs;
	for (int i = 0; i < parser->numChunks; ++i) {
		allChunkIDs.push_back(i);
	}

	// random seed number and then shuffle
	srand((unsigned)time(NULL));
	std::random_shuffle(allChunkIDs.begin(), allChunkIDs.end());

	// initialize the accounting data structure with another vector.
	for(int i = 0; i < parser->numServers; i++){
		vector<ContentProperty> chunkProperties;
		this->serverContentAssignment.push_back(chunkProperties);
	}

	// assign chunk to servers
	int rrPointer = 0;
	int partitionSize = parser->numChunks/(1.0*parser->numServers);
	for(int i = 0; i < parser->numChunks; i++){
		int currServer = i / partitionSize;		
		if(currServer >= this->serverContentAssignment.size()){
			currServer = (rrPointer++)%parser->numServers;
		}
		
		ContentProperty currChunk{-1, allChunkIDs[i], parser->chunkSize};
		this->serverContentAssignment[currServer].push_back(currChunk);
	}
}

void ServerWorkload::generateFileWorkload(const ConfigParser* parser){
	// reset the server chunk assignment accounting
	this->serverContentAssignment.clear();
	this->obj2Chunk.clear();

	// push all the file ids first
	vector<int> allFileIDs;
	for (int i = 0; i < parser->numFiles; ++i) {
		allFileIDs.push_back(i);
	}

	// random seed number and then shuffle to generate ranked file 
	// list as popularity ranking
	srand((unsigned)time(NULL));
	std::random_shuffle(allFileIDs.begin(), allFileIDs.end());

	// initialize the accounting data structure with another vector.
	for(int i = 0; i < parser->numServers; i++){
		vector<ContentProperty> fileProperties;
		this->serverContentAssignment.push_back(fileProperties);
	}

	// assign chunk to servers
	int rrPointer = 0;
	int partitionSize = parser->numFiles/(1.0*parser->numServers);
	for(int i = 0; i < parser->numFiles; i++){
		int currNumChunks = rand() % (parser->maxFileNumChunks - parser->minFileNumChunks + 1) + parser->minFileNumChunks;
		int currServer = i / partitionSize;
		if(currServer >= this->serverContentAssignment.size()){
			currServer = (rrPointer++)%parser->numServers;
		}

		for(int j = 0; j < currNumChunks; j++){
			ContentProperty currChunk{allFileIDs[i], j, parser->chunkSize};
			this->serverContentAssignment[currServer].push_back(currChunk);
		}
		this->obj2Chunk[allFileIDs[i]] = currNumChunks - 1;
	}
}

void ServerWorkload::generateVideoWorkload(const ConfigParser* parser){
	// reset the server chunk assignment accounting
	this->serverContentAssignment.clear();
	this->obj2Chunk.clear();

	// push all the video ids first
	vector<int> allVideoIDs;
	for (int i = 0; i < parser->numVideos; ++i) {
		allVideoIDs.push_back(i);
	}

	// random seed number and then shuffle
	srand((unsigned)time(NULL));
	std::random_shuffle(allVideoIDs.begin(), allVideoIDs.end());

	// initialize the accounting data structure with another vector.
	for(int i = 0; i < parser->numServers; i++){
		vector<ContentProperty> videoProperties;
		this->serverContentAssignment.push_back(videoProperties);
	}

	// assign chunk to servers
	int rrPointer = 0;
	int partitionSize = parser->numVideos/(1.0*parser->numServers);
	for(int i = 0; i < parser->numVideos; i++){
		int currNumChunks = rand() % (parser->maxVideoNumChunks - parser->minVideoNumChunks + 1) + parser->minVideoNumChunks;
		int currServer = i / partitionSize;
		if(currServer >= this->serverContentAssignment.size()){
			currServer = (rrPointer++)%parser->numServers;
		}

		for(int j = 0; j < currNumChunks; j++){
			ContentProperty currChunk{allVideoIDs[i], j, parser->chunkSize};
			this->serverContentAssignment[currServer].push_back(currChunk);
		}
		this->obj2Chunk[allVideoIDs[i]] = currNumChunks - 1;
	}
}

void ServerWorkload::printServerContentAssignment() const{
	for(int i = 0; i < this->serverContentAssignment.size(); i++){
		cout << "Server " << i << " has" << endl;

		for(int j = 0; j < this->serverContentAssignment[i].size(); j++){
			cout << "\tobject id: " << this->serverContentAssignment[i][j].objectId << " chunk id: " << this->serverContentAssignment[i][j].chunkId << 
				" chunk size: " << this->serverContentAssignment[i][j].chunkSize << endl;
		}
	}
}

void ServerWorkload::save(const char* templateFileName){
	for(int i = 0; i < this->serverContentAssignment.size(); i++){
		ostringstream ss;
		ss << templateFileName << i+1 << ".dat";
		string currFileName = ss.str();
		
		ofstream currFile;
		currFile.open(currFileName);

		for(int j = 0; j < this->serverContentAssignment[i].size(); j++){
			currFile << this->serverContentAssignment[i][j].objectId << " " << this->serverContentAssignment[i][j].chunkId << " "
				<< this->serverContentAssignment[i][j].chunkSize << endl;
		}
		currFile.close();
	}
}

map<int, int> ServerWorkload::getObj2Chunk() const{
	return this->obj2Chunk;
}