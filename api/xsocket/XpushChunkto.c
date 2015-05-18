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
** 
** XpushChunkto sends a datagram style chunk to the specified address. The final intent of
** the address should be a valid SID or HID. i.e., AD->HID (gets cached) AD->HID->SID (gets cached and send to application through xrecvchunkfrom)
** 
** Unlike a standard socket, XpushChunkto() is only valid on Xsockets of
** type XSOCK_CHUNK. 
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
	  
	  if ((rc = XputChunk(ctx, buf, len, info)) < 0)
		return rc;
	  printf("CID put: %s\n", info->cid);


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
		LOGF("truncating... requested size (%lu) is larger than XIA_MAXBUF (%d)\n", 
				len, XIA_MAXBUF);
		len = XIA_MAXBUF;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XPUSHCHUNKTO);
	unsigned seq = seqNo(ctx->sockfd);
	xsm.set_sequence(seq);

	xia::X_Pushchunkto_Msg *x_pushchunkto = xsm.mutable_x_pushchunkto();

	// FIXME: validate addr
	Graph g((sockaddr_x*)addr);
	std::string s = g.dag_string();
	
//	printf("Destination DAG in API Call: %s \n", s.c_str());

	x_pushchunkto->set_ddag(s.c_str());
	x_pushchunkto->set_payload((const char*)buf, len);
	
	x_pushchunkto->set_contextid(ctx->contextID);
	x_pushchunkto->set_ttl(ctx->ttl);
	x_pushchunkto->set_cachesize(ctx->cacheSize);
	x_pushchunkto->set_cachepolicy(ctx->cachePolicy);
	x_pushchunkto->set_cid(info->cid);
	x_pushchunkto->set_length(sizeof(info->cid));
	printf("PushChunk Message CID: %s\n", x_pushchunkto->cid().c_str());

	if ((rc = click_send(ctx->sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	// process the reply from click
	if ((rc = click_status(ctx->sockfd, seq)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
	}

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
** XpushFileto() calls XpushChunkto() internally and has the same requiremts as that 
** function.
**
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

