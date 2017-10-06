#include "ncid-demo-client.h"

NCIDDemoClient::NCIDDemoClient()
{
	std::cout << "Starting NCIDDemoClient" << std::endl;
	std::cout << "Opening a socket to publish data" << std::endl;

	if((_sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		throw "Unable to create socket for client";
	}

	// Get a handle to Xcache
	if(XcacheHandleInit(&_xcache)) {
		throw "Unable to talk to xcache";
	}

	std::cout << "Started NCIDDemoClient" << std::endl;
}

NCIDDemoClient::~NCIDDemoClient()
{
	// Cleanup state before exiting
	if (_sockfd >= 0) {
		Xclose(_sockfd);
	}
}

int NCIDDemoClient::fetch()
{
	// For now, just create a chunk of data
	std::string data("Some Random Data");

	// Store it as a named chunk
	int ret;
	void *buf;
	int flags = XCF_BLOCK | XCF_CACHE;
	buf = NULL;
	std::string name = _publisher + "/" + _content_name;
	std::cout << "Fetching: " << name << std::endl;
	ret = XfetchNamedChunk(&_xcache, &buf, flags, name.c_str());
	if (ret < 0) {
		std::cout << "Failed fetching named chunk" << std::endl;
		return ret;
	}
	if(ret > 0) {
		std::string chunk((const char *)buf, (size_t)ret);
		std::cout << "Chunk contains:" << chunk << ":" << std::endl;
	}

	if(buf) {
		free(buf);
	}
	return 0;
}

int main()
{
	NCIDDemoClient *client = new NCIDDemoClient();
	int retval = client->fetch();
	delete client;
	return retval;
}
