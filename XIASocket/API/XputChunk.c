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

/**
 * @brief Initialize Context for PutCID operation.
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
 * @param size Max size for the local cache 
 *
 * @return A struct that contains PutCID context
 *
 * */
struct CIDContext_t *XallocCacheSlice(unsigned policy, unsigned ttl, unsigned size) {
    int sockfd = Xsocket(XSOCK_CHUNK);
    if(sockfd < 0) {
        LOG("Unable to allocate the cache slice.\n");
        return NULL;
    } else {

		// FIXME: validate the parameters
		// FIXME: contextID is going to need to be somethign else so we can have multiple ones
		// FIXME: add protobuf for this instead of rolling it up with the putChunk call

        struct CIDContext_t *newCtx = (struct CIDContext_t *)malloc(sizeof(struct CIDContext_t));

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
int XfreeCacheSlice(struct CIDContext_t *ctx)
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
 * @param TTL Time to live in seconds; 0 means permanent(differ by policy)
 * @param cStat Struct to hold metadata returned from PutCID
 *
 * @return 0 on success; -1 on error
 *
 */
// FIXME: set errno on errors
//  use Xutil to send recv buffers for cleaner code
// return error if cstat is null (or why would we let it be?)
// what's going on with ttl changing? (testCID app)

int XputChunk(const struct CIDContext_t *ctx, const char *data, unsigned length, struct cStat_t *cStat)
{
    if(ctx == NULL) {
        return -1;
    }
    int contextID = ctx->contextID;
    int sockfd = ctx->sockfd;
    int cacheSize = ctx->cacheSize;
    int cachePolicy = ctx->cachePolicy;
    int retVal;
	int ttl = ctx->ttl;
    char buffer[MAXBUFLEN];
    socklen_t addrLen;
    struct addrinfo hints, *servInfo;
    struct sockaddr_in theirAddr;
    memset(&hints , 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((retVal = getaddrinfo(CLICKDATAADDRESS,
                              CLICKDATAPORT, &hints, &servInfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retVal));
        return -1;
    }

    //Build request
    xia::XSocketMsg _socketMsg;
    _socketMsg.set_type(xia::XPUTCHUNK);
    xia::X_Putchunk_Msg *_msg = _socketMsg.mutable_x_putchunk();
    _msg->set_contextid(contextID);
    _msg->set_payload((const char *)data, length);
    fprintf(stderr, "DATA: %s,[size]%d\n", data, length);
    _msg->set_ttl(ttl);
    _msg->set_cachesize(cacheSize);
    _msg->set_cachepolicy(cachePolicy);

    std::string pBuf;
    _socketMsg.SerializeToString(&pBuf);
    if((retVal = sendto(sockfd, pBuf.c_str(), pBuf.size(), 0,
                        servInfo->ai_addr, servInfo->ai_addrlen)) < 0) {
        fprintf(stderr, "XputChunk send failed\n");
        return -1;
    }
    freeaddrinfo(servInfo);

    //Process reply
    addrLen = sizeof(theirAddr);
    bzero(buffer, MAXBUFLEN);
    if((retVal = recvfrom(sockfd, buffer, MAXBUFLEN - 1, 0,
                          (struct sockaddr *)&theirAddr, &addrLen)) < 0) {
        fprintf(stderr, "XputChunk recv failed\n");
        return -1;
    }
    xia::XSocketMsg _socketMsgReply;
    std::string bufStr(buffer, retVal);
    _socketMsgReply.ParseFromString(bufStr);
    if(_socketMsgReply.type() == xia::XPUTCHUNK) {
        if(cStat != NULL) {
            xia::X_Putchunk_Msg *_msgReply = _socketMsgReply.mutable_x_putchunk();
            cStat->size = _msgReply->payload().size();
            strcpy(cStat->CID, _msgReply->cid().c_str());
            cStat->ttl= _msgReply->ttl();
            cStat->timestamp.tv_sec=_msgReply->timestamp();
            cStat->timestamp.tv_usec = 0;
        }
        return 0;
    } else {
printf("got an error!!!\n");
        return -1;
    }
}

/**
 * @brief Publish content from a FILE.
 * 
 * This function is only a wrapper. It chops the file and hand them to XputChunk().
 *
 * @param ctx Pointer to a context
 * @param fp Pointer to FILE
 * @param chunkSize Size to chop the FILE into
 * @param hash If NULL, hash is calculated as SHA1 digest. (Recommemded)
 *             If not NULL, all file chunks will have the same hash as supplied.
 *             This is ususally not desirable.
 * @param TTL Time to live for each chunk in seconds; 
 *            0 means permanent(differ by policy)
 * @param cStat Array of struct to hold metadata returned from PutCID
 * @param numcStat Length of cStat. If chunks put is larger than numcStat, extra cStat
 *                 will be truncated.
 *
 * @return On success, the number of chunks published; -1 on error
 */
// FIXME: validate ctx, fp, chunksize > 0, cstat not null, numcstat at least as big as needed
// stat the file and figure out how many chunks there will be, ensure cstat is at least that big
// or.... let this code create the cstat array, and add a routine for the caller to free it

int XputFile(struct CIDContext_t *ctx, FILE *fp, unsigned chunkSize, unsigned TTL, struct cStat_t *cStat, unsigned numcStat){
    if(fp==NULL || chunkSize <=0 ){
        return -1;
    }
    unsigned numChunk=0;
    int retVal=-1;
    unsigned numRead;
    char *buffer=(char *)malloc(chunkSize*sizeof(char));
    while(1){
        numRead=fread(buffer, sizeof(char), chunkSize, fp);
		printf("read %d bytes\n", numRead);
        if(ferror(fp)!=0){
            fprintf(stderr, "XputFile I/O failed\n");
            retVal=-1;
            break;
        }
        if(numRead>0){
            if(numChunk<numcStat){
                retVal=XputChunk(ctx, buffer, numRead, TTL, &cStat[numChunk++]);
				printf("XputChunk returns %d\n", retVal);
            }else{
                retVal=XputChunk(ctx, buffer, numRead, TTL, NULL);
				printf("XputChunk 2 (why would we do this?) returns %d\n", retVal);
            }
//            numChunk++;
            if(retVal<0){
                fprintf(stderr, "XputFile Chunk[%d] failed\n", numChunk);
                break;
            }
        }
        if(feof(fp)){
            retVal=numChunk;
            break;
        }
    }
    free(buffer);
    return retVal;
}

/**
 * @brief Remove a published content from local cache
 * 
 * @param ctx Pointer to a context
 * @param cStat Struct that hold CID to remove
 *
 * @return 0 on success; -1 on error
 *
 */
int XremoveChunk(struct CIDContext_t *ctx, struct cStat_t *cStat)
{
    if(cStat == NULL || ctx == NULL) {
        return -1;
    }
    int sockfd = ctx->sockfd;
    char hashBuf[1024];
    int retVal;
    char buffer[2048];
    socklen_t addrLen;
    struct addrinfo hints, *servInfo;
    struct sockaddr_in theirAddr;
    memset(&hints , 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if ((retVal = getaddrinfo(CLICKDATAADDRESS,
                              CLICKDATAPORT, &hints, &servInfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retVal));
        return -1;
    }

    strcpy(hashBuf, cStat->CID);
    xia::XSocketMsg _socketMsg;
    _socketMsg.set_type(xia::XREMOVECHUNK);
    xia::X_Removechunk_Msg *_msg = _socketMsg.mutable_x_removechunk();
    _msg->set_contextid(ctx->contextID);
    _msg->set_cid(hashBuf);

    std::string pBuf;
    _socketMsg.SerializeToString(&pBuf);
    if((retVal = sendto(sockfd, pBuf.c_str(), pBuf.size(), 0,
                        servInfo->ai_addr, servInfo->ai_addrlen)) < 0) {
        fprintf(stderr, "XremoveChunk send failed\n");
        freeaddrinfo(servInfo);
        return -1;
    }
    freeaddrinfo(servInfo);

    //Process reply
    addrLen = sizeof(theirAddr);
    if((retVal = recvfrom(sockfd, buffer, MAXBUFLEN - 1, 0,
                          (struct sockaddr *)&theirAddr, &addrLen)) < 0) {
        fprintf(stderr, "XremoveChunk recv failed\n");
        return -1;
    }
    xia::XSocketMsg _socketMsgReply;
    std::string bufStr(buffer, retVal);
    _socketMsgReply.ParseFromString(bufStr);
    if(_socketMsgReply.type() == xia::XREMOVECHUNK) {
        xia::X_Removechunk_Msg *_msgReply = _socketMsgReply.mutable_x_removechunk();
        return _msgReply->status();
    } else {
        return -1;
    }
}
