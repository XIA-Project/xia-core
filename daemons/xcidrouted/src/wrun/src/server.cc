#include "../../common/constants.h"
#include "../../common/cmdline.h"

#include "parser.h"
#include "server.h"

using namespace std;

static char myAD[MAX_XID_SIZE];
static char myHID[MAX_XID_SIZE];
static char my4ID[MAX_XID_SIZE];

static int serverSock = -1;
static XcacheHandle xcache;

static vector<ChunkInfo> chunks;

void registerServer(){
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];

	// create a socket, and listen for incoming connections
	if ((serverSock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0){
		printf("Unable to create the listening socket\n");
		exit(0);
	}

	// read the localhost AD and HID
	if (XreadLocalHostAddr(serverSock, myAD, sizeof(myAD), myHID, sizeof(myHID), my4ID, sizeof(my4ID)) < 0 ){
		printf("Reading localhost address\n");
		exit(0);
	}

	cout << "local host address: " << endl;
	cout << "\t AD:" << myAD << endl;
	cout << "\t HID" << myHID << endl;

	struct addrinfo *ai;

	if (XmakeNewSID(sid_string, sizeof(sid_string))) {
		printf("Unable to create a temporary SID\n");
		exit(0);
	}

	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) != 0){
		printf("getaddrinfo failure!\n");
		exit(0);
	}

	sockaddr_x *dag = (sockaddr_x*)ai->ai_addr;
	if (XregisterName(XIA_CID_ROUTING_SERVER_NAME, dag) < 0 ){
		printf("error registering name: %s\n", XIA_CID_ROUTING_SERVER_NAME);
		exit(0);
	}

	if (Xbind(serverSock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(serverSock);
		printf("Unable to bind to the dag.\n");
		exit(0);
	}

	Xlisten(serverSock, 5);
}

void makeRandomChunk(char* chunk, int chunkSize){
	// clear the data first
	bzero (chunk, chunkSize);

	for (int i = 0; i < chunkSize; i++){
		chunk[i] = (char)random();
	}
}

void putRandomChunk(const char* chunk, int chunkSize, sockaddr_x *addr){
	if (XputChunk(&xcache, chunk, (size_t)chunkSize, addr) < 0){
		printf("put chunk failed\n");
		exit(0);
	}
}

void publishWorkload(ServerWorkloadParser* parser){
	int currChunkId, currObjectId, currChunkSize;
	char currChunk[XIA_MAXBUF];
	char url[URL_LENGTH];
	sockaddr_x addr;

	for(unsigned i = 0; i < parser->chunks.size(); i++){
		currObjectId = parser->chunks[i].objectId;
		currChunkId = parser->chunks[i].chunkId;
		currChunkSize = KB(parser->chunks[i].chunkSize);
		bzero(currChunk, sizeof(currChunk));
		bzero(url, URL_LENGTH);

		// make the chunk first
		makeRandomChunk(currChunk, currChunkSize);
		// put the chunk
		putRandomChunk(currChunk, currChunkSize, &addr);
		// copy back the url
		dag_to_url(url, URL_LENGTH, &addr);

		//make a new chunk info
		ChunkInfo chunkInfo;
		chunkInfo.objectId = currObjectId;
		chunkInfo.chunkId = currChunkId;
		strcpy(chunkInfo.dagUrl, url);

		// append it to our list
		chunks.push_back(chunkInfo);
	}
}

void saveChunksCID(int workloadId, string cidMapDir){
	string CID_MAP_FILE_TEMPLATE_STR(CID_MAP_FILE_TEMPLATE);
	string fileName = cidMapDir + "/" + CID_MAP_FILE_TEMPLATE_STR + to_string(workloadId) + ".dat";

	ofstream currFile;
	currFile.open(fileName);

	for (unsigned i = 0; i < chunks.size(); i++){
		currFile << chunks[i].objectId <<  " " << chunks[i].chunkId << " " << chunks[i].dagUrl << endl;
	}

	currFile.close();
}

int main(int argc, char *argv[]) {
	// build the command line parser
	cmdline::parser cmdParser;
	cmdParser.add<string>("workload", 'w', "workload directory", false, WORKLOAD_DIR);
	cmdParser.add<int>("workload-id", 'i', "which workload should this server read from ", false, 1);
	cmdParser.add<string>("chunkid-cid-mapping", 'c', "directory to store the chunk id to chunk cid mapping", false, CID_MAP_DIR);

	// check the command line argument
	cmdParser.parse_check(argc, argv);

	// get the desired command line argument
	string workloadDir = cmdParser.get<string>("workload");
	int workloadId = cmdParser.get<int>("workload-id");
	string cidMapDir = cmdParser.get<string>("chunkid-cid-mapping");

	// construct the full path to workload file
	string SERVER_FILE_TEMPLATE(SERVER_WORKLOAD_FILE_TEMPLATE);
	string worklaodFile = workloadDir + "/" + SERVER_FILE_TEMPLATE + to_string(workloadId) + ".dat";

	// register the receiver
	registerServer();
	// initialize the xcache
	XcacheHandleInit(&xcache);

	// build the parser
	ServerWorkloadParser* parser = new ServerWorkloadParser;
	// parse the workload
	parser->parse(worklaodFile.c_str());
	//publish the workload
	publishWorkload(parser);

	//save the chunk id to CID mapping
	saveChunksCID(workloadId, cidMapDir);

	delete parser;

	// don't terminate the program since we still have connection to xcache
	while(1){};

	return 0;
}