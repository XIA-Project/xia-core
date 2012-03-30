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

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <sys/stat.h>
#include <errno.h>

/**
 * @brief Initialize Context for the XputChunk,XputFile,XputBuffer functions.
 *
 * Init a context to store meta data, e.g. cache policy.
 * Serve as an application handler when putting content.
 * This can replace the old socket() call, because we don't
 * really need a socket, but an identifier to the application.
 *
 * The identifier uses getpid(), but can be replaced by any
 * unique ID.
 *
 * @param policy Policy to use for the local cache(defined in Xsocket.h)
 * @param ttl Time to live in seconds; 0 means permanent(differ by policy)
 * @param size Max size for the local cache 
 *
 * @return A struct that contains PutCID context
 *
 * */
ChunkContext *XallocCacheSlice(unsigned policy, unsigned ttl, unsigned size) {
    int sockfd = Xsocket(XSOCK_CHUNK);
    if(sockfd < 0) {
        LOG("Unable to allocate the cache slice.\n");
        return NULL;
    } else {

		// FIXME: validate the parameters
		// FIXME: contextID is going to need to be somethign else so we can have multiple ones
		// FIXME: add protobuf for this instead of rolling it up with the putChunk call

        ChunkContext *newCtx = (ChunkContext *)malloc(sizeof(ChunkContext));

        newCtx->contextID = getpid();
        newCtx->cachePolicy = policy;
        newCtx->cacheSize = size;
		newCtx->ttl = ttl;
        newCtx->sockfd = sockfd;
        LOGF("New CTX: sock,policy,size=%d,%d,%d\n", sockfd, policy, size);
        return newCtx;
    }
}

/*!
** @brief Release a cache slice
**
** @warning This does not yet exist on the click side!
**
*/
int XfreeCacheSlice(ChunkContext *ctx)
{
	if (!ctx)
		return 0;

	// FIXME: what kind of teardown has to happen inside of click?????

	int rc = Xclose(ctx->sockfd);
	free(ctx);
	return rc;
}

/**
 * @brief Publish content from a buffer.
 * 
 * @param ctx Pointer to a context
 * @param data Data to put
 * @param length Length of the data buffer
 * @param info Struct to hold metadata returned from PutCID
 *
 * @return 0 on success; -1 on error
 *
 */
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

	if ((rc = click_data(ctx->sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// process the reply from click
	if ((rc = click_reply(ctx->sockfd, buffer, sizeof(buffer))) < 0) {
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

/**
 * @brief Publish content from a FILE.
 * 
 * This function is only a wrapper. It chops the file and hand them to XputChunk().
 *
 * @param ctx Pointer to a context
 * @param fname File to send
 * @param chunkSize Size to chop the FILE into
 * @param info Array of struct to hold metadata returned from PutCID
 *
 * @return On success, the number of chunks published; -1 on error
 */
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

/**
 * @brief Remove a published content from local cache
 * 
 * @param ctx Pointer to a context
 * @param info Struct that hold CID to remove
 *
 * @return 0 on success; -1 on error
 *
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

	if ((rc = click_data(ctx->sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// process the reply from click
	if ((rc = click_reply(ctx->sockfd, buffer, sizeof(buffer))) < 0) {
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


void XfreeChunkInfo(ChunkInfo *infop)
{
	if (infop)
		free(infop);
}

