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
** @file XputChunk.c
** @brief implements XputChunk(), XputFile(), XputBuffer(), XremoveChunk(),
** XallocCacheSlice(),XfreeCacheSlice(), and XfreeChunkInfo()
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <sys/stat.h>
#include <errno.h>

/*!
** @brief Allocate content cache space for use by the XputChunk(),
** XputFile(), and XputBuffer() functions.
**
** Allocate a slice of content cache storage in the local machine to
** store content we make available. Multiple cache slices may be allocated
** by a single application for different purposes. Once the cache slice is
** full, old content will be purged on a FIFO basis to make room for new
** content chunks.`
**
** @param policy Policy to use for the local cache (not currently used, the
** always uses a FIFO policy at this time).
** @param ttl Time to live in seconds; 0 means permanent. Once the TTL is
** elapsed content will be automatically flushed from the cache. Content may
** be flushed before th TTL expires if the cache becomes full.
** @param size Max size for the cache slice
**
** @returns A struct that contains the cache slice context.
** @returns NULL if the slice can't be allocated.
**
** @warning, As currently implemented, this function uses the process id
** as the the cache slice identifier. This needs to be changed so that an
** can create multiple slices.
**
** @note, we may want to consider using 0 to specify an slice with no upper bound.
**
*/
ChunkContext *XallocCacheSlice(unsigned policy, unsigned ttl, unsigned size) {
    int sockfd = Xsocket(AF_XIA, XSOCK_CHUNK, 0);
    if(sockfd < 0) {
        LOG("Unable to allocate the cache slice.\n");
        return NULL;
    } else {

		// FIXME: contextID is going to need to be somethign else so we can have multiple ones
		// FIXME: add protobuf for this instead of rolling it up with the putChunk call

        ChunkContext *newCtx = (ChunkContext *)malloc(sizeof(ChunkContext));

        newCtx->contextID = getpid();
        newCtx->cachePolicy = policy;
        newCtx->cacheSize = size;
		newCtx->ttl = ttl;
        newCtx->sockfd = sockfd;
//        LOGF("New CTX: sock,policy,size=%d,%d,%d\n", sockfd, policy, size);
        return newCtx;
    }
}

/*!
** @brief Release a cache slice.
**
** This function closes the socket used to communicate with the click
** and frees the ChunkContext that was allocated.
**
** @param ctx - the cache slice to free
**
** @returns 0 on success
** @returns -1 on error with errno set.
**
** @note This does not tear down the content cache itself. It will live until
** the content in it expires. To clear the cache in the current release,
** XremoveChunk() can be called for each chunk of data.
*/
int XfreeCacheSlice(ChunkContext *ctx)
{
	if (!ctx)
		return 0;

	int rc = Xclose(ctx->sockfd);
	free(ctx);
	return rc;
}

/*!
** @brief Publish a single chunk of content.
**
** XputChunk() makes a single chunk of data available on the network.
** On success, the CID of the chunk is set to the 40 character hash of the
** content data. The CID is not a full DAG, and must be converted to a DAG
** before the client applicatation can request it, otherwise an error will
** occur.
**
** If the chunk causes the cache slice to grow too large, the oldest content
** chunk(s) will be reoved to make enough space for this chunk.
**
** @param ctx Pointer to the cache slice where this chunk will be stored
** @param data The data to published. The size of data must be less than
** XIA_MAXCHUNK or an error will be returned.
** @param length Length of the data buffer
** @param info Struct to hold metadata returned, include the chunk identifier (CID)
**
** @returns 0 on success
** @returns -1 on error
**
**/
int XputChunk(const ChunkContext *ctx, const char *data, unsigned length, ChunkInfo *info)
{
    int rc;
    char buffer[MAXBUFLEN];


	if (length > XIA_MAXCHUNK) {
		errno = EMSGSIZE;
		LOGF("Chunk size of %d is too large\n", length);
		return -1;
	}

    if(ctx == NULL || data == NULL || info == NULL) {
		errno = EFAULT;
		LOG("NULL pointer");
        return -1;
    }

	if (length == 0)
		return 0;

    //Build request
    xia::XSocketMsg xsm;
    xsm.set_type(xia::XPUTCHUNK);

    xia::X_Putchunk_Msg *_msg = xsm.mutable_x_putchunk();

    _msg->set_contextid(ctx->contextID);
    _msg->set_payload((const char *)data, length);
    _msg->set_ttl(ctx->ttl);
    _msg->set_cachesize(ctx->cacheSize);
    _msg->set_cachepolicy(ctx->cachePolicy);

	if ((rc = click_send(ctx->sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// process the reply from click
	if ((rc = click_reply(ctx->sockfd, xia::XPUTCHUNK, buffer, sizeof(buffer))) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

    xia::XSocketMsg _socketMsgReply;
    std::string bufStr(buffer, rc);
    _socketMsgReply.ParseFromString(bufStr);
    if(_socketMsgReply.type() == xia::XPUTCHUNK) {
		xia::X_Putchunk_Msg *_msgReply = _socketMsgReply.mutable_x_putchunk();
		info->size = _msgReply->payload().size();
		strcpy(info->cid, _msgReply->cid().c_str());
		info->ttl= _msgReply->ttl();
		info->timestamp.tv_sec=_msgReply->timestamp();
		info->timestamp.tv_usec = 0;
        return 0;
    } else {
        return -1;
    }
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
int XputFile(ChunkContext *ctx, const char *fname, unsigned chunkSize, ChunkInfo **info)
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

			if ((rc = XputChunk(ctx, buf, count, &infoList[i])) < 0)
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
int XputBuffer(ChunkContext *ctx, const char *data, unsigned len, unsigned chunkSize, ChunkInfo **info)
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

		if ((rc = XputChunk(ctx, p, count, &infoList[i])) < 0)
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
** @brief Remove a chunk of content from the cache.
**
** This function will remove the specified CID from the content cache. A
** successful return code will be returned regardless of whether or not the
** chunk was already expired out of the cache. The CID parameter must be
** the value returned from one of the Xput... functions, a full DAG will not be
** recognized as a valid identifier.
**
** @param ctx The cache slice containing the content.
** @param cid The CID to remove. This should only be the 40 character
** hash identifier of the CID, not the entire DAG.
**
** @returns 0 on success
** returns -1 on error
**
*/
int XremoveChunk(ChunkContext *ctx, const char *cid)
{
    char buffer[2048];
    int rc;

    if(cid == NULL || ctx == NULL) {
		errno = EFAULT;
        return -1;
    }

    xia::XSocketMsg xsm;
    xsm.set_type(xia::XREMOVECHUNK);
    xia::X_Removechunk_Msg *_msg = xsm.mutable_x_removechunk();

    _msg->set_contextid(ctx->contextID);
    _msg->set_cid(cid);

	if ((rc = click_send(ctx->sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// process the reply from click
	if ((rc = click_reply(ctx->sockfd, xia::XPUTCHUNK, buffer, sizeof(buffer))) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

    xia::XSocketMsg _socketMsgReply;
    std::string bufStr(buffer, rc);
    _socketMsgReply.ParseFromString(bufStr);

    if(_socketMsgReply.type() == xia::XREMOVECHUNK) {
        xia::X_Removechunk_Msg *_msgReply = _socketMsgReply.mutable_x_removechunk();
        return _msgReply->status();
    } else {
        return -1;
    }
}

/*!
** @brief Delete an array of ChunkInfo structures.
**
** This function should be called when the application is done with the
** ChunkInfo array returned from XputFile() or XputBuffer() to release the
** memory.
**
** @param infop The memory to free
**
** @returns void
**
*/
void XfreeChunkInfo(ChunkInfo *infop)
{
	if (infop)
		free(infop);
}

