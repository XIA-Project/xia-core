#ifndef _SERVER_H
#define _SERVER_H

#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>

#include "xcache.h"
#include "Xsocket.h"
#include "dagaddr.hpp"
#include "dagaddr.h"
#include "Xkeys.h"

#define MAX_XID_SIZE 100
#define MAX_DAG_SIZE 512

#define MB(__mb) (KB(__mb) * 1024)
#define KB(__kb) ((__kb) * 1024)

const int URL_LENGTH = 256;

typedef struct _ChunkInfo{
	int objectId;
	int chunkId;
	char dagUrl[URL_LENGTH];
} ChunkInfo;

void registerServer();

void makeRandomChunk(char* chunk, int chunkSize);

void putRandomChunk(const char* chunk, int chunkSize, sockaddr_x *addr);

void publishWorkload(ServerWorkloadParser* parser);

void saveChunksCID(int workloadId, string cidMapDir);

#endif