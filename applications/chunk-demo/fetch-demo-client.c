#include "fetch-demo-client.h"

#include "xcache_cmd.pb.h"

#include <chrono>
#include <thread>

FetchDemoClient *client;

// Callback function that Xcache API will call on receiving an async event
void gotChunkData(XcacheHandle* /*h*/, int /*event*/,
		void *data, size_t datalen)
{
	// 'data' contains a protobuf of type notif_contents
	notif_contents contents;
	std::string buffer((char *)data, datalen);
	contents.ParseFromString(buffer);
	// notify get_chunk() that the data is ready
	std::cout << "Got " << contents.cid() << std::endl;
	std::cout << "of length " << contents.data().length() << std::endl;
	client->got_chunk(contents.cid(), contents.data());
}

FetchDemoClient::FetchDemoClient()
{
	std::cout << "Starting FetchDemoClient" << std::endl;

	_chunk_ready = false;
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
	if(_proxy_id > 0) {
		std::cout << "Stopping proxy" << std::endl;
		XendProxy(&_xcache, _proxy_id);
		std::cout << "Stopped proxy" << std::endl;
	}
	XcacheHandleDestroy(&_xcache);
}


void FetchDemoClient::got_chunk(const std::string &cid,
		const std::string &data)
{
	// Hold lock and copy data over to this object
	std::lock_guard<std::mutex> lock(_cr_lock);
	_cid.assign(cid);
	_data.assign(data);
	_chunk_ready = true;
	_cr.notify_all();
}

void FetchDemoClient::get_chunk(std::string &cid, std::string &data)
{
	// Wait for chunk to be ready - condition _cr
	std::unique_lock<std::mutex> lk(_cr_lock);
	_cr.wait(lk, [&] { return _chunk_ready == true;});
	// Copy over the cid and data to user buffers
	cid.assign(_cid);
	data.assign(_data);
	_cid = "";
	_data = "";
	_chunk_ready = false;
}

// Create a new proxy and submit request for a chunk
// @returns proxy_id returned by XnewProxy, use for stopping proxy later
int FetchDemoClient::request(std::string &chunk_dag, std::string &fs_dag)
{
	// Start a new PushProxy that will accept chunks when they are
	// pushed back to us by the Fetching Service
	std::string proxyaddr;
	if ((_proxy_id = XnewProxy(&_xcache, proxyaddr)) < 0) {
		std::cout << "Error starting proxy to accept pushed chunk" << std::endl;
		return -1;
	}
	// Now connect to Fetching Service and request the chunk
	if(XrequestPushedChunk(chunk_dag, fs_dag, proxyaddr)) {
		std::cout << "Error requesting chunk fetch" << std::endl;
		return -1;
	}
	return _proxy_id;
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

	client = new FetchDemoClient();
	int retval = client->request(chunk_dag, fs_dag);
	if(retval) {
		std::cout << "Error submitting chunk fetch request" << std::endl;
	}

	// Block, waiting for the chunk to be delivered by PushProxy to us
	std::string cid;
	std::string data;
	client->get_chunk(cid, data);
	std::cout << "main: Got " << cid << std::endl;
	std::cout << "main: of length " << data.length() << std::endl;
	delete client;
	std::cout << "main: Done." << std::endl;

	return retval;
}
