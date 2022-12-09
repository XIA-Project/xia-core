/**
 * Demonstrate use of XnewProxy() API call
 *
 * Starts a PushProxy inside Xcache and leaves it running.
 *
 * @returns 0 on success, -1 on failure
 */
#include <Xsocket.h>
#include <xcache.h>
#include <dagaddr.hpp>

int main()
{
	XcacheHandle _xcache;
	if (XcacheHandleInit(&_xcache)) {
		printf("ERROR talking to xcache\n");
		return -1;
	}

	// Request a new push proxy
	std::string proxyaddr;
	int proxy_id = XnewProxy(&_xcache, proxyaddr);
	if (proxy_id == -1) {
		printf("ERROR starting proxy for pushed chunks\n");
		return -1;
	}
	printf("Proxy started at address: %s\n", proxyaddr.c_str());

	if (XcacheHandleDestroy(&_xcache)) {
		printf("ERROR closing session with Xcache\n");
		return -1;
	}
	printf("New proxy for pushed chunks should be running now\n");
	return 0;
}
