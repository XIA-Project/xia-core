// XIA Includes
#include <dagaddr.hpp>
#include <xcache.h>

// System includes
#include <stdlib.h> 	// random()
#include <stdio.h>		// printf()

#define CHUNKSIZE 512

int main()
{
	char chunk[CHUNKSIZE];
	int i;
	XcacheHandle xcache;
	int state = 0;
	int rc = -1;
	sockaddr_x addr;
	Graph *g;

	// Connect to Xcache
	if(XcacheHandleInit(&xcache)) {
		printf("ERROR: talking to xcache\n");
		goto make_random_chunk_done;
	}
	state = 1;	// Destroy XcacheHandle

	// Create a buffer with random data as chunk contents
	printf("Creating random chunk\n");
	for(i=0;i<CHUNKSIZE;i++) {
		chunk[i] = (char) random();
	}

	// Put the chunk into xcache
	if(XputChunk(&xcache, (const char *)chunk, (size_t)CHUNKSIZE, &addr) < 0) {
		printf("ERROR: XputChunk failed\n");
		goto make_random_chunk_done;
	}

	// Print out the address of our chunk
	g = new Graph(&addr);
	printf("Chunk address: %s\n", g->dag_string().c_str());
	rc = 0;

make_random_chunk_done:
	switch(state) {
		case 1: XcacheHandleDestroy(&xcache);
	};
	return rc;
}
