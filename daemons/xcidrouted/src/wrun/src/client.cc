#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#include "parser.h"
#include "client.h"
#include "../../common/constants.h"
#include "../../common/cmdline.h"

using namespace std;

static XcacheHandle xcache;
static map<string, sockaddr_x> chunkIdToChunkDAG;
static vector<RequestStats> requests;
static set<string> cidsSoFar;

void parseChunkIdToCIDMap(string cidMapDir){
	DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(cidMapDir.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if(strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0
                    && strstr(ent->d_name, CID_MAP_FILE_TEMPLATE) != NULL){
            	string fullPath = cidMapDir + "/" + ent->d_name;

				string line;
				ifstream infile(fullPath);

				while (getline(infile, line)) {
					if(line == ""){
						continue;
					}

					vector<string> parsedLine = WorkloadParser::splitStringDelimiter((char*)line.c_str(), " ");
					string objectId = parsedLine[0];
					string chunkId = parsedLine[1];
					string chunkDAG = parsedLine[2];

					sockaddr_x chunkDAGSock;
					url_to_dag(&chunkDAGSock, (char*)chunkDAG.c_str(), strlen(chunkDAG.c_str()));
					
					Graph g(chunkDAGSock);
					Node finalIntent = g.get_final_intent();
					string finalIntentStr = finalIntent.to_string();

					if(cidsSoFar.find(finalIntentStr) != cidsSoFar.end()){
						printf("same CID as before, not allowed\n");
						closedir(dir);
						exit(0);
					}
					cidsSoFar.insert(finalIntentStr);

					chunkIdToChunkDAG[objectId + "-" + chunkId] = chunkDAGSock;
				}
            }
        }
        closedir (dir);
    } else {
        printf("cannot open directory\n");
        exit(0);
    }

}

void fetchWorkload(ClientWorkloadParser* parser){
	int currChunkId, currChunkSize, currObjectId;
	double currChunkDelay, elapsedTime;
	char currBuf[XIA_MAXBUF];
	sockaddr_x currAddr;
	struct timeval t1, t2;

	for(unsigned i = 0; i < parser->requests.size(); i++){
		currObjectId = parser->requests[i].objectId;
		currChunkId = parser->requests[i].chunkId;
		currChunkSize = KB(parser->requests[i].chunkSize);	// chunk size is in KB by default
		currChunkDelay = parser->requests[i].delay;			// chunk delay is in miliseconds
		currAddr = chunkIdToChunkDAG[to_string(currObjectId) + "-" + to_string(currChunkId)];
		bzero(currBuf, sizeof(currBuf));

		// first sleep for a pre-defined delay in microseconds
		usleep((useconds_t)currChunkDelay*1000);

		gettimeofday(&t1, NULL);
		if(XfetchChunk(&xcache, currBuf, currChunkSize, XCF_BLOCK, &currAddr, sizeof(currAddr)) < 0) {
			printf("XfetchChunk Failed\n");
			exit(0);
		}
		gettimeofday(&t2, NULL);
		elapsedTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
        elapsedTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
		
		// chunk latency is in ms
		RequestStats prop = {currObjectId, currChunkId, currChunkSize, elapsedTime, XgetPrevFetchHopCount()};
		requests.push_back(prop);
	}
}

void saveStats(int workloadId){
	string STATS_DIR_STR(STATS_DIR);
	string CLIENT_STATS_FILE_TEMPLATE_STR(CLIENT_STATS_FILE_TEMPLATE);
	string fullPath = STATS_DIR_STR + "/" + CLIENT_STATS_FILE_TEMPLATE_STR + to_string(workloadId) + ".dat";

	ofstream currFile;
	currFile.open(fullPath);

	for(unsigned i = 0; i < requests.size(); i++){
		int objectId = requests[i].objectId;
		int chunkId = requests[i].chunkId;
		int chunkSize = requests[i].chunkSize;
		double chunkLatency = requests[i].delay;
		int hopCount = requests[i].hopCount;

		currFile << objectId << " " << chunkId << " " << chunkSize << " " << chunkLatency << " " << hopCount << endl;
 	}

	currFile.close();
}

int main(int argc, char *argv[]) {
	// build the command line parser
	cmdline::parser cmdParser;
	cmdParser.add<string>("workload", 'w', "workload directory", false, WORKLOAD_DIR);
	cmdParser.add<int>("workload-id", 'i', "which client workload should this client read from ", false, 1);
	cmdParser.add<string>("chunkid-cid-mapping", 'c', "directory to store the chunk id to chunk cid mapping", false, CID_MAP_DIR);

	// check the command line argument
	cmdParser.parse_check(argc, argv);

	// get the desired command line argument
	string workloadDir = cmdParser.get<string>("workload");
	int workloadId = cmdParser.get<int>("workload-id");
	string cidMapDir = cmdParser.get<string>("chunkid-cid-mapping");

	// construct the full path to workload file
	string CLIENT_FILE_TEMPLATE(CLIENT_WORKLOAD_FILE_TEMPLATE);
	string worklaodFile = workloadDir + "/" + CLIENT_FILE_TEMPLATE + to_string(workloadId) + ".dat";

	// initialize the xcache
	XcacheHandleInit(&xcache);

	// build the parser
	ClientWorkloadParser* parser = new ClientWorkloadParser;
	// parse the workload
	parser->parse(worklaodFile.c_str());

	// parse the server generated chunk id to chunk cid mapping
	parseChunkIdToCIDMap(cidMapDir);
	// fetch the workload
	fetchWorkload(parser);
	// save the statistics from client side
	saveStats(workloadId);

	delete parser;
	return 0;
}