/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
** @file XpushChunkto.c
** @brief implements XpushChunkto()
*/
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>
#include "dagaddr.hpp"
#include <sys/stat.h>

/*!
** @brief Sends a datagram chunk message on an Xsocket
**
** Internally calls XputChunk 
** XpushChunkto sends a datagram style chunk to the specified address. The final intent of
** the address should be a valid SID or HID.
** 
** Unlike a standard socket, XpushChunkto() is only valid on Xsockets of
** type XSOCK_CHUNK. FIXME: Maybe there should be chunk_datagram
**
** If the buffer is too large (bigger than XIA_MAXCHUNK), XpushChunkto() will return an error.
**
** @param ChunkContext Chunk context including socket, ttl, etc
** @param buf the data to send
** @param len lenngth of the data to send. The
** Xsendto api is limited to sending at most XIA_MAXBUF bytes.
** @param flags (This is not currently used but is kept to be compatible
** with the standard sendto socket call.
** @param addr address (SID) to send the datagram to
** @param addrlen length of the DAG
** @param ChunkInfo cid, ttl, len, etc
**
** @returns number of bytes sent on success
** @returns -1 on failure with errno set to an error compatible with those
** returned by the standard sendto call.
**
*/
int XpushChunkto(const ChunkContext *ctx, const char *buf, size_t len, int flags,
		const struct sockaddr *addr, socklen_t addrlen, ChunkInfo *info)
{
	
	int rc;
// 	char buffer[MAXBUFLEN];
	
	if ((rc = XputChunk(ctx, buf, len, info)) < 0)
		return rc;
	

/*	if(ctx == NULL || buf == NULL || info == NULL || !addr)*/
	if(!addr){
			errno = EFAULT;
			LOG("NULL pointer");
		return -1;
	}

// 	if (len == 0)
// 		return 0;


	if (addrlen < sizeof(sockaddr_x)) {
		errno = EINVAL;
		return -1;
	}

	if (flags != 0) {
		LOG("the flags parameter is not currently supported");
		errno = EINVAL;
		return -1;
	}

	if (validateSocket(ctx->sockfd, XSOCK_CHUNK, EOPNOTSUPP) < 0) {
		LOGF("Socket %d must be a chunk socket", ctx->sockfd);
		return -1;
	}

	// if buf is too big, send only what we can
	if (len > XIA_MAXBUF) {
		LOGF("truncating... requested size (%d) is larger than XIA_MAXBUF (%d)\n", 
				len, XIA_MAXBUF);
		len = XIA_MAXBUF;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XPUSHCHUNKTO);

	xia::X_Pushchunkto_Msg *x_pushchunkto = xsm.mutable_x_pushchunkto();

	// FIXME: validate addr
	Graph g((sockaddr_x*)addr);
	std::string s = g.dag_string();
	
// 	printf("destination dag: %s \n", s.c_str());

	x_pushchunkto->set_ddag(s.c_str());
	x_pushchunkto->set_payload((const char*)buf, len);
	
	x_pushchunkto->set_contextid(ctx->contextID);
	x_pushchunkto->set_ttl(ctx->ttl);
	x_pushchunkto->set_cachesize(ctx->cacheSize);
	x_pushchunkto->set_cachepolicy(ctx->cachePolicy);
	x_pushchunkto->set_cid(info->cid);
	x_pushchunkto->set_length(sizeof(info->cid));
	printf("info->cid: %s\n", info->cid);
	printf("CID for Push right after setting it: %s\n", x_pushchunkto->cid().c_str());

	if ((rc = click_send(ctx->sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// because we don't have queueing or seperate control and data sockets, we 
	// can't get status back reliably on a datagram socket as multiple peers
	// could be talking to it at the same time and the control messages can get
	// mixed up with the data packets. So just assume that all went well and tell
	// the caller we sent the data

	return len;
}



int XpushBufferto(const ChunkContext *ctx, const char *data, size_t len, int flags,
		const struct sockaddr *addr, socklen_t addrlen, ChunkInfo **info, unsigned chunkSize)
{
		
	ChunkInfo *infoList;
	unsigned numChunks;
	unsigned i;
	int rc;
	int count;
	char *buf;
	const char *p;

	if (ctx == NULL || data == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (chunkSize == 0)
		chunkSize =  DEFAULT_CHUNK_SIZE;
	else if (chunkSize > XIA_MAXBUF)
		chunkSize = XIA_MAXBUF;

	numChunks = len / chunkSize;
	if (len % chunkSize)
		numChunks ++;

	if (!(infoList = (ChunkInfo*)calloc(numChunks, chunkSize))) {
		return -1;
	}

	if (!(buf = (char*)malloc(chunkSize))) {
		free(infoList);
		return -1;
	}

	p = data;
	for (i = 0; i < numChunks; i++) {
		count = MIN(len, chunkSize);
		// XputChunk(ctx, p, count, &infoList[i]))
		if ((rc =XpushChunkto(ctx, p, count, flags, addr, addrlen, &infoList[i]) < 0))
			break;
		len -= count;
		p += chunkSize;
	}

	if (i != numChunks) {
		// FIXME: something happened, what do we want to do in this case?
		rc = -1;
	}
	else
		rc = i;

	*info = infoList;
	free(buf);

	return rc;
}



/*!
** @brief Publish a file by breaking it into one or more content chunks.
**
** XputFile() calls XputChunk() internally and has the same requiremts as that 
** function.
**
** On success, the CID of the chunk is set to the 40 character hash of the
** content data. The CID is not a full DAG, and must be converted to a DAG
** before the client applicatation can request it, otherwise an error will
** occur.
**
** If the file causes the cache slice to grow too large, the oldest content 
** chunk(s) will be reoved to make enough space for the new chunk(s).
**
** @param ctx Pointer to the cache slice where this chunk will be stored
** @param fname The file to publish.
** @param chunkSize The maximum requested size of each chunk. This value
** must not be larger than XIA_MAXCHUNK or an error will be returned.
** @param info a pointer to an array of ChunkInfo structures. The memory for
** this array is allocated by the XputFile() function on success and should
** be free'd with the XfreeChunkInfo() function when it is no longer needed.
**
** @returns The number of chunks created on success with info pointing to an 
** allocated array of ChunkInfo structures.
** @returns -1 on error
**
**/
int XpushFileto(const ChunkContext *ctx, const char *fname, int flags,
		const struct sockaddr *addr, socklen_t addrlen, ChunkInfo **info, unsigned chunkSize)
{
	FILE *fp;
	struct stat fs;
	ChunkInfo *infoList;
	unsigned numChunks;
	unsigned i;
	int rc;
	int count;
	char *buf;

	if (ctx == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (fname == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (chunkSize == 0)
		chunkSize =  DEFAULT_CHUNK_SIZE;
	else if (chunkSize > XIA_MAXBUF)
		chunkSize = XIA_MAXBUF;

	if (stat(fname, &fs) != 0)
		return -1;

	if (!(fp= fopen(fname, "rb")))
		return -1;

	numChunks = fs.st_size / chunkSize;
	if (fs.st_size % chunkSize)
		numChunks ++;
	//FIXME: this should be numChunks, sizeof(ChunkInfo)
	if (!(infoList = (ChunkInfo*)calloc(numChunks, chunkSize))) {
		fclose(fp);
		return -1;
	}

	if (!(buf = (char*)malloc(chunkSize))) {
		free(infoList);
		fclose(fp);
		return -1;
	}

	i = 0;
	while (!feof(fp)) {
	
		if ((count = fread(buf, sizeof(char), chunkSize, fp)) > 0) {
			if ((rc = XpushChunkto(ctx, buf, count, flags, addr, addrlen, &infoList[i])) < 0)
				break;
			i++;
		}
	}

	if (i != numChunks) {
		// FIXME: something happened, what do we want to do in this case?
		rc = -1;
	}
	else
		rc = i;

	*info = infoList;
	fclose(fp);
	free(buf);

	return rc;
}





/*!
** @brief Publish a file by breaking it into one or more content chunks.
**
** XputBuffer() calls XputChunk() internally and has the same requiremts as that 
** function.
**
** On success, the CID of the chunk is set to the 40 character hash of the
** content data. The CID is not a full DAG, and must be converted to a DAG
** before the client applicatation can request it, otherwise an error will
** occur.
**
** If the file causes the cache slice to grow too large, the oldest content 
** chunk(s) will be reoved to make enough space for the new chunk(s).
**
** @param ctx Pointer to the cache slice where this chunk will be stored
** @param data The data buffer to be published
** @param len length of the data buffer
** @param chunkSize The maximum requested size of each chunk. This value
** must not be larger than XIA_MAXCHUNK or an error will be returned.
** @param info a pointer to an array of ChunkInfo structures. The memory for
** this array is allocated by the XputBuffer() function on success and should
** be free'd with the XfreeChunkInfo() function when it is no longer needed.
**
** @returns The number of chunks created on success with info pointing to an 
** allocated array of ChunkInfo structures.
** @returns -1 on error
**
**/
// int XputBuffer(ChunkContext *ctx, const char *data, unsigned len, unsigned chunkSize, ChunkInfo **info)
// {
// 	ChunkInfo *infoList;
// 	unsigned numChunks;
// 	unsigned i;
// 	int rc;
// 	int count;
// 	char *buf;
// 	const char *p;
// 
// 	if (ctx == NULL || data == NULL) {
// 		errno = EFAULT;
// 		return -1;
// 	}
// 
// 	if (chunkSize == 0)
// 		chunkSize =  DEFAULT_CHUNK_SIZE;
// 	else if (chunkSize > XIA_MAXBUF)
// 		chunkSize = XIA_MAXBUF;
// 
// 	numChunks = len / chunkSize;
// 	if (len % chunkSize)
// 		numChunks ++;
// 
// 	if (!(infoList = (ChunkInfo*)calloc(numChunks, chunkSize))) {
// 		return -1;
// 	}
// 
// 	if (!(buf = (char*)malloc(chunkSize))) {
// 		free(infoList);
// 		return -1;
// 	}
// 
// 	p = data;
// 	for (i = 0; i < numChunks; i++) {
// 		count = MIN(len, chunkSize);
// 
// 		if ((rc = XputChunk(ctx, p, count, &infoList[i])) < 0)
// 			break;
// 		len -= count;
// 		p += chunkSize;
// 	}
// 
// 	if (i != numChunks) {
// 		// FIXME: something happened, what do we want to do in this case?
// 		rc = -1;
// 	}
// 	else
// 		rc = i;
// 
// 	*info = infoList;
// 	free(buf);
// 
// 	return rc;
// }




/*!
** @brief Publish a file by breaking it into one or more content chunks.
**
** XputFile() calls XputChunk() internally and has the same requiremts as that 
** function.
**
** On success, the CID of the chunk is set to the 40 character hash of the
** content data. The CID is not a full DAG, and must be converted to a DAG
** before the client applicatation can request it, otherwise an error will
** occur.
**
** If the file causes the cache slice to grow too large, the oldest content 
** chunk(s) will be reoved to make enough space for the new chunk(s).
**
** @param ctx Pointer to the cache slice where this chunk will be stored
** @param fname The file to publish.
** @param chunkSize The maximum requested size of each chunk. This value
** must not be larger than XIA_MAXCHUNK or an error will be returned.
** @param info a pointer to an array of ChunkInfo structures. The memory for
** this array is allocated by the XputFile() function on success and should
** be free'd with the XfreeChunkInfo() function when it is no longer needed.
**
** @returns The number of chunks created on success with info pointing to an 
** allocated array of ChunkInfo structures.
** @returns -1 on error
**
**/
// int XputFile(ChunkContext *ctx, const char *fname, unsigned chunkSize, ChunkInfo **info)
// {
// 	FILE *fp;
// 	struct stat fs;
// 	ChunkInfo *infoList;
// 	unsigned numChunks;
// 	unsigned i;
// 	int rc;
// 	int count;
// 	char *buf;
// 
// 	if (ctx == NULL) {
// 		errno = EFAULT;
// 		return -1;
// 	}
// 
// 	if (fname == NULL) {
// 		errno = EFAULT;
// 		return -1;
// 	}
// 
// 	if (chunkSize == 0)
// 		chunkSize =  DEFAULT_CHUNK_SIZE;
// 	else if (chunkSize > XIA_MAXBUF)
// 		chunkSize = XIA_MAXBUF;
// 
// 	if (stat(fname, &fs) != 0)
// 		return -1;
// 
// 	if (!(fp= fopen(fname, "rb")))
// 		return -1;
// 
// 	numChunks = fs.st_size / chunkSize;
// 	if (fs.st_size % chunkSize)
// 		numChunks ++;
// 	//FIXME: this should be numChunks, sizeof(ChunkInfo)
// 	if (!(infoList = (ChunkInfo*)calloc(numChunks, chunkSize))) {
// 		fclose(fp);
// 		return -1;
// 	}
// 
// 	if (!(buf = (char*)malloc(chunkSize))) {
// 		free(infoList);
// 		fclose(fp);
// 		return -1;
// 	}
// 
// 	i = 0;
// 	while (!feof(fp)) {
// 	
// 		if ((count = fread(buf, sizeof(char), chunkSize, fp)) > 0) {
// 
// 			if ((rc = XputChunk(ctx, buf, count, &infoList[i])) < 0)
// 				break;
// 			i++;
// 		}
// 	}
// 
// 	if (i != numChunks) {
// 		// FIXME: something happened, what do we want to do in this case?
// 		rc = -1;
// 	}
// 	else
// 		rc = i;
// 
// 	*info = infoList;
// 	fclose(fp);
// 	free(buf);
// 
// 	return rc;
// }


