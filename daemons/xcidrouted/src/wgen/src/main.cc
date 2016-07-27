#include <iostream>
#include <string>
#include "../../common/constants.h"
#include "../../common/cmdline.h"
#include "config.h"
#include "workload.h"

void printWorkloadConfig(ConfigParser *parser){
	cout << "workload configuration: " << endl;
	cout << "\tchunkSize: " << parser->chunkSize << endl;
	cout << "\tnumClients: " << parser->numClients << endl;
	cout << "\tnumServers: " << parser->numServers << endl;
	cout << "\trateRequests: " << parser->rateRequests << endl;
	cout << "\tnumRequests: " << parser->numRequests << endl;
	cout << "\tzipfAlpha: " << parser->zipfAlpha << endl;

	if(parser->workloadType.compare(WORKLOAD_TYPE_CHUNK) == 0){
		cout << "\tworkload type: " << WORKLOAD_TYPE_CHUNK << endl;
		cout << "\tnumChunks: " << parser->numChunks << endl;
	} else if (parser->workloadType.compare(WORKLOAD_TYPE_FILE) == 0){
		cout << "\tworkload type: " << WORKLOAD_TYPE_FILE << endl;
		cout << "\tnumFiles: " << parser->numFiles << endl;
		cout << "\tminFileNumChunks: " << parser->minFileNumChunks << endl;
		cout << "\tmaxFileNumChunks: " << parser->maxFileNumChunks << endl;
	} else if (parser->workloadType.compare(WORKLOAD_TYPE_VIDEO) == 0){
		cout << "\tworkload type: " << WORKLOAD_TYPE_VIDEO << endl;
		cout << "\tnumVideos: " << parser->numVideos << endl;
		cout << "\tminVideoNumChunks: " << parser->minVideoNumChunks << endl;
		cout << "\tmaxVideoNumChunks: " << parser->maxVideoNumChunks << endl;
		cout << "\tvideoChunkZipfAlpha: " << parser->videoChunkZipfAlpha << endl;
	}
}

int main(int argc, char *argv[]) {
	// build the command line parser
	cmdline::parser cmdParser;
	cmdParser.add<string>("workload-config", 'w', "location of the workload configuration", false, CONFIG_FILE_NAME);
	cmdParser.add<string>("server-workload", 's', "location to store the generated server workload", false, SERVER_WORKLOAD_TEMPLATE_FILE_NAME);
	cmdParser.add<string>("client-workload", 'c', "location to store the generated client workload", false, CLIENT_WORKLOAD_TEMPLATE_FILE_NAME);

	// check the command line argument
	cmdParser.parse_check(argc, argv);

	// get the desired command line argument
	string workloadConfig = cmdParser.get<string>("workload-config");
	string serverWorkload = cmdParser.get<string>("server-workload");
	string clientWorkload = cmdParser.get<string>("client-workload");

	// build the parser and workload
	ConfigParser *parser = new ConfigParser;

	// parse the workload
	parser->parse(workloadConfig.c_str());

	// print out some helpful information about the workload
	printWorkloadConfig(parser);

	// create workload generator
	ServerWorkload *sworklaod = new ServerWorkload(parser);
	ClientWorkload *cworklaod = new ClientWorkload(parser, sworklaod->getObj2Chunk());

	// first generate server side workload
	sworklaod->save(serverWorkload.c_str());

	// second generate client side workload
	cworklaod->save(clientWorkload.c_str());

	// deallocate the resources
	delete parser;
	delete sworklaod;
	delete cworklaod;
	return 0;
}