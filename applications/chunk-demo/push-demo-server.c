#include "push-demo-server.h"

#define CHUNKSIZE 512

PushDemoServer::PushDemoServer()
{
	std::cout << "Starting PushDemoServer" << std::endl;
	std::cout << "Opening a socket to publish data" << std::endl;

	if((_sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		throw "Unable to create socket for server";
	}

	// Get a handle to Xcache
	if(XcacheHandleInit(&_xcache)) {
		throw "Unable to talk to xcache";
	}

	std::cout << "Started PushDemoServer" << std::endl;
}

PushDemoServer::~PushDemoServer()
{
	// Cleanup state before exiting
	if (_sockfd >= 0) {
		Xclose(_sockfd);
	}
}

int PushDemoServer::createRandomChunk(sockaddr_x *addr)
{
	char chunk[CHUNKSIZE];
	int i;
	std::cout << "Creating random chunk" << std::endl;
	for(i=0; i<CHUNKSIZE; i++) {
		chunk[i] = (char)random();
	}
	std::cout << "Created random chunk of size " << CHUNKSIZE << std::endl;
	if(XputChunk(&_xcache, (const char *)chunk, (size_t)CHUNKSIZE, addr) < 0) {
		std::cout << "XputChunk failed" << std::endl;
		return -1;
	}
	std::cout << "Placed random chunk into cache" << std::endl;
	return 0;
}

int PushDemoServer::serve(sockaddr_x *remoteAddr)
{
	sockaddr_x chunkaddr;

	// For now, just create a chunk of data
	if(createRandomChunk(&chunkaddr)) {
		std::cout << "Failed creating a random chunk" << std::endl;
		return -1;
	}
	Graph g(&chunkaddr);
	std::cout << "Created chunk: " << g.dag_string() << std::endl;

	// Push the chunk to the provided remote address
	if(XpushChunk(&_xcache, &chunkaddr, remoteAddr)) {
		std::cout << "Failed pushing chunk to remote addr" << std::endl;
		return -1;
	}

	std::cout << "Chunk was pushed to remote addr" << std::endl;
	return 0;
}

int main(int argc, char **argv)
{
	if(argc != 2) {
		std::cout << "Usage:" << argv[0] << " remote_dag" << std::endl;
		return -1;
	}

	// Convert argument into remote address
	Graph g(argv[1]);
	sockaddr_x remoteAddr;
	g.fill_sockaddr(&remoteAddr);
	std::cout << "Remote addr: " << g.dag_string() << std::endl;

	PushDemoServer *server = new PushDemoServer();
	int retval = server->serve(&remoteAddr);
	delete server;
	return retval;
}
