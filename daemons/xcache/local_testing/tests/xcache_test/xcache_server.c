#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
#include "dagaddr.hpp"
#include <assert.h>
#include "Xkeys.h"
#include "xcache.h"

int main(void)
{
	struct xcacheChunk *info = NULL;
    struct xcacheSlice slice;

	XcacheInit();
    if(XcacheAllocateSlice(&slice, 20000000, 1000, 0) < 0)
		return -1;

	XcachePutFile(&slice, "./test", 1000, &info);

	while(1);
	return 0;
}
