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
	if (XnewProxy(&_xcache)) {
		printf("ERROR starting proxy for pushed chunks\n");
		return -1;
	}
	if (XcacheHandleDestroy(&_xcache)) {
		printf("ERROR closing session with Xcache\n");
		return -1;
	}
	printf("New proxy for pushed chunks should be running now\n");
	return 0;
}
