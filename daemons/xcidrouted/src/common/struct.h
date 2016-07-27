#ifndef _STRUCT_H
#define _STRUCT_H

typedef struct {
	int objectId; 		// file/video id; -1 if we are doing chunk workload
	int chunkId;		// unique integer chunk id
	int chunkSize; 		// in KB
} ContentProperty;

typedef struct {
	int objectId; 		// file/video id; -1 if we are doing chunk workload
	int chunkId;		// unique integer chunk id
	int chunkSize;		// in KB
	double delay;		// in ms
} RequestProperty;

#endif