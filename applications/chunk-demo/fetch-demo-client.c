#include "fetch-demo-client.h"

FetchDemoClient::FetchDemoClient()
{
	std::cout << "Starting FetchDemoClient" << std::endl;

	// Get a handle to Xcache
	if(XcacheHandleInit(&_xcache)) {
		throw "Unable to talk to xcache";
	}
}

FetchDemoClient::~FetchDemoClient()
{
	// Cleanup state before exiting
	XcacheHandleDestroy(&_xcache);
}

int FetchDemoClient::request(std::string &chunk_dag, std::string &fs_dag)
{
	// Start a new PushProxy that will accept chunks when they are
	// pushed back to us by the Fetching Service
	std::string proxyaddr;
	if (XnewProxy(&_xcache, proxyaddr)) {
		std::cout << "Error starting proxy to accept pushed chunk" << std::endl;
		return -1;
	}
	// Now connect to Fetching Service and request the chunk
	if(XrequestPushedChunk(chunk_dag, fs_dag, proxyaddr)) {
		std::cout << "Error requesting chunk fetch" << std::endl;
		return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	// User must provide DAG of chunk to retrieve
	if(argc != 3) {
		std::cout << "Usage: " << argv[0] << " <chunk_dag>"
			<< " <FetchService_addr>" << std::endl;
		return -1;
	}
	// TODO: Sanity check arguments provided by user
	// Simply buliding a Graph would be ok
	std::string chunk_dag(argv[1]);
	std::string fs_dag(argv[2]);

	FetchDemoClient *client = new FetchDemoClient();
	int retval = client->request(chunk_dag, fs_dag);
	if(retval) {
		std::cout << "Error submitting request for a chunk fetch" << std::endl;
	}

	// We simply submit a request for the chunk and exit at this time.
	// In future, we'll block waiting for requested chunk to arrive
	delete client;
	return retval;
}
