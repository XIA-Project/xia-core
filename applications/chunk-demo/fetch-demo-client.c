#include "fetch-demo-client.h"

#include "xcache_cmd.pb.h"

#include <chrono>
#include <thread>

FetchDemoClient::FetchDemoClient()
{
	std::cout << "Starting FetchDemoClient" << std::endl;

	// Get a handle to Xcache
	if(XcacheHandleInit(&_xcache)) {
		throw "Unable to talk to xcache";
	}
	// Register the callback for arriving chunk
	XregisterNotif(XCE_CHUNKCONTENTS, gotChunkData);
	XlaunchNotifThread(&_xcache);
}

FetchDemoClient::~FetchDemoClient()
{
	// Cleanup state before exiting
	XcacheHandleDestroy(&_xcache);
}

void FetchDemoClient::gotChunkData(XcacheHandle* /*h*/, int /*event*/,
		void *data, size_t datalen)
{
	// 'data' contains a protobuf of type notif_contents
	notif_contents contents;
	std::string buffer((char *)data, datalen);
	contents.ParseFromString(buffer);
	// notify get_chunk() that the data is ready
	std::cout << "Got " << contents.cid() << std::endl;
	std::cout << "of length " << contents.data().length() << std::endl;

}

void FetchDemoClient::get_chunk(std::string &cid, std::string &data)
{
	// Wait for notification from getChunkData that data is ready
	// Fill in data and cid for caller
	std::this_thread::sleep_for(std::chrono::seconds(20));
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

	// Block, waiting for the chunk to be delivered by PushProxy to us
	std::string cid;
	std::string data;
	client->get_chunk(cid, data);
	std::cout << "Got " << cid << std::endl;
	std::cout << "of length " << data.length() << std::endl;
	delete client;

	return retval;
}
