#include "ncid-demo-server.h"
#include <memory>	// std::unique_ptr

NCIDDemoServer::NCIDDemoServer()
{
	std::cout << "Starting NCIDDemoServer" << std::endl;
	std::cout << "Opening a socket to publish data" << std::endl;

	if((_sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		throw "Unable to create socket for server";
	}

	// Get a handle to Xcache
	if(XcacheHandleInit(&_xcache)) {
		throw "Unable to talk to xcache";
	}

	std::cout << "Started NCIDDemoServer" << std::endl;
}

NCIDDemoServer::~NCIDDemoServer()
{
	// Cleanup state before exiting
	if (_sockfd >= 0) {
		Xclose(_sockfd);
	}
}

int NCIDDemoServer::serve()
{
	// For now, just create a chunk of data
	std::string data("Some Random Data");

	// Store it as a named chunk
	std::cout << "Putting a chunk by its name into xcache" << std::endl;
	if(XputNamedChunk(&_xcache, data.c_str(), data.size(),
			_content_name.c_str(), _publisher.c_str())) {
		std::cout << "Failed publishing named chunk" << std::endl;
		return -1;
	}
	std::cout << "Named chunk was placed into cache" << std::endl;
	return 0;
}

int main()
{
	std::unique_ptr<NCIDDemoServer> server(new NCIDDemoServer());
	return server->serve();
}
