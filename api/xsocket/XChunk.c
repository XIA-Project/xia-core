/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
// xtransport.hh
void XrequestChunk(unsigned short _sport, WritablePacket *p_in);
void XgetChunkStatus(unsigned short _sport);
void XreadChunk(unsigned short _sport); 
void XremoveChunk(unsigned short _sport);
void XputChunk(unsigned short _sport);
void XpushChunkto(unsigned short _sport, WritablePacket *p_in);

// 
// Xsocket.h
extern int XrequestChunk(int sockfd, char* dag, size_t dagLen);
extern int XrequestChunks(int sockfd, const ChunkStatus *chunks, int numChunks);
extern int XgetChunkStatus(int sockfd, char* dag, size_t dagLen);
extern int XgetChunkStatuses(int sockfd, ChunkStatus *statusList, int numCids);
extern int XreadChunk(int sockfd, void *rbuf, size_t len, int flags, char *cid, size_t cidLen);
extern int XpushChunkto(const ChunkContext* ctx, const char* buf, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen, ChunkInfo* info);
extern int XpushBufferto(const ChunkContext *ctx, const char *data, size_t len, int flags, const struct sockaddr *addr, socklen_t addrlen, ChunkInfo **info, unsigned chunkSize);
extern int XpushFileto(const ChunkContext *ctx, const char *fname, int flags, const struct sockaddr *addr, socklen_t addrlen, ChunkInfo **info, unsigned chunkSize);
extern int XrecvChunkfrom(int sockfd, void* rbuf, size_t len, int flags, ChunkInfo* ci);

extern ChunkContext *XallocCacheSlice(unsigned policy, unsigned ttl, unsigned size);
extern int XfreeCacheSlice(ChunkContext *ctx);
extern int XputChunk(const ChunkContext *ctx, const char *data, unsigned length, ChunkInfo *info);
extern int XputFile(ChunkContext *ctx, const char *fname, unsigned chunkSize, ChunkInfo **infoList);
extern int XputBuffer(ChunkContext *ctx, const char *, unsigned size, unsigned chunkSize, ChunkInfo **infoList);
extern int XremoveChunk(ChunkContext *ctx, const char *cid);
extern void XfreeChunkInfo(ChunkInfo *infoList);
*/

/*
/home/xia/xia-core/api/xsocket/XgetChunkStatus.c
/home/xia/xia-core/api/xsocket/XpushChunkto.c
/home/xia/xia-core/api/xsocket/XputChunk.c
/home/xia/xia-core/api/xsocket/XreadChunk.c
/home/xia/xia-core/api/xsocket/XrecvChunkfrom.c
/home/xia/xia-core/api/xsocket/XrequestChunk.c 
*/

#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include <errno.h>
#include "dagaddr.hpp"
#include <sys/stat.h>

/*!
** @file XrequestChunk.c
** @brief implements XrequestChunk() and XrequestChunks()
*/

/*!
** @brief request that a content chunk be loaded into the local machine's
** content cache.
**
** XrequestChunk() is called by a client application to load a content chunk 
** into the XIA content cache. It does not return the requested data, it only
** causes the chunk to be loaded into the local content cache. XgetChunkStatus() 
** may be called to get the status of the chunk to determine when it becomes 
** available. Once the chunk is ready to be read, XreadChunk() should be called 
** get the actual content chunk data.
**
** XrequestChunk() is a simple wrapper around the XrequestChunks() API call.
**
** @param sockfd the control socket (must be of type XSOCK_CHUNK)
** @param dag Content ID of this chunk
** @param dagLen length of dag (currently not used)
**
** @returns 0 on success
** @returns -1 if the requested chunk could not be located or a socket error
** occurred. If the error is a socket error, errno set will be set with an 
** appropriate code.
*/
int XrequestChunk(int sockfd, char* dag, size_t /* dagLen */) {
	ChunkStatus cs;

	cs.cid = dag;
	cs.cidLen = strlen(dag);

	return XrequestChunks(sockfd, &cs, 1);
}

/*!
** @brief request that a list of content chunks be loaded into the local 
** machine's content cache.
**
** XrequestChunks() is called by a client application to load a content chunk 
** into the XIA content cache. It does not return the requested data, it only
** causes the chunk to be loaded into the local content cache. XgetChunkStatuses() 
** may be called to get the status of the chunk to determine when it becomes 
** available. Once the chunk is ready to be read, XreadChunk() should be called 
** get the actual content chunk data.
**
** XrequestChunk() can be used when only a single chunk is requested.
**
** @param sockfd the control socket (must be of type XSOCK_CHUNK)
** @param chunks A list of content DAGs to retrieve
** @param numChunks number of CIDs in the chunk list
**
** @returns 0 on success
** @returns -1 if one or more of the requested chunks could not be located 
** or a socket error occurred. If the error is a socket error, errno set 
** will be set with an appropriate code.
*/
int XrequestChunks(int sockfd, const ChunkStatus *chunks, int numChunks) {
	int rc;
	const char *buf = "Chunk request";//Maybe send more useful information here.

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket", sockfd);
		return -1;
	}

	if (numChunks == 0)
		return 0;

	if (!chunks) {
		LOG("null pointer error!");
		errno = EFAULT;
		return -1;
	}
	
	// If the DAG list is too long for a UDP packet to click, replace with multiple calls
	// TODO: Make this more precise 
	if (numChunks > 300) 	{
		rc = 0;
		int i;
		for (i = 0; i < numChunks; i += 300) {
			int num = (numChunks-i > 300) ? 300 : numChunks-i;
			int rv = XrequestChunks(sockfd, &chunks[i], num);

			if (rv == -1) {
				perror("XrequestChunk(): requestChunk failed");
				return(-1);
			} else {
				rc += rv;
			}
		}
		return rc;
	}
	
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XREQUESTCHUNK);

	xia::X_Requestchunk_Msg *x_requestchunk_msg = xsm.mutable_x_requestchunk();
  
	for (int i = 0; i < numChunks; i++) {
		if (chunks[i].cid != NULL)
			x_requestchunk_msg->add_dag(chunks[i].cid);
		else {
			LOGF("NULL pointer at chunks[%d]\n", i);
		}
	}

	if (x_requestchunk_msg->dag_size() == 0) {
		// FIXME: what error should this relate to?
		errno = EFAULT;
		LOG("No dags specified\n");
		return -1;
	}

	x_requestchunk_msg->set_payload((const char*)buf, strlen(buf)+1);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

#if 0	
	// process the reply from click
	if ((rc = click_reply2(sockfd, &type)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}
#endif

	return 0;
}

/*!
** @file XgetChunkStatus.c
** @brief implements XgetChunkStatus() and XgetChunkStatuses()
*/

#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"


/*!
** @brief Checks the status of the specified CID. 
**
** XgetChunkStatus returns an integer indicating if the specified content 
** chunk is available to be read. It is a simple wrapper around the 
** XgetChunkStatuses() function which does the actual work.
**
** @note This function Should be called after calling XrequestChunk() or 
** XrequestChunks(). Otherwise the content chunk will never be loaded into
** the content cache and will result in a REQUEST_FAILED error.
**
** @param sockfd the control socket (must be of type XSOCK_CHUNK)
** @param dag Content ID of the chunk to check. This should be a full dag
** for the desired chunk, not just the CID.
** @param dagLen length of dag (currently not used)
**
** @returns READY_TO_READ if the requested chunk is ready to be read.
** @returns INVALID_HASH if the CID hash does not match the content payload.
** @returns WAITING_FOR_CHUNK if the requested chunk is still in transit.
** @returns REQUEST_FAILED if the specified chunk has not been requested
** @returns -1 if a socket error occurs. In that case errno is set with the appropriate code.
*/
int XgetChunkStatus(int sockfd, char* dag, size_t /* dagLen */) {
	ChunkStatus cs;

	cs.cid = dag;
	cs.cidLen = strlen(dag);

	return XgetChunkStatuses(sockfd, &cs, 1);
}

/*
** @brief Checks the status for each of the requested CIDs.
**
** XgetChunkStatuses updates the cDAGv list with the status for each
** of the requested CIDs. An overall status value is returned, and
** the cDAGv list can be examined to check the status of each individual CID.
**
** @note This function Should be called after calling XrequestChunk() or 
** XrequestChunks(). Otherwise the content chunk will never be loaded into
** the content cache and will result in a REQUEST_FAILED error.
**
** @param sockfd - the control socket (must be of type XSOCK_CHUNK)
** @param cDAGv - list of CIDs to check. On return, also  contains the status for 
** each of the specified CIDs.
** @param numCIDs - number of CIDs in cDAGv
**
** @returns a bitfield indicating the status of the chunks.
** @returns if return equals READY_TO_READ, all chunks are avilable to read
** @returns otherwise the return value contains a bitfield of status codes
** @returns if REQUEST_FAILED is set, one or more of the requested chunks could not be found
** @returns if WAITING_FOR_CHUNK is set, one or more of the requested chunks is still in transit
** @returns if INVALID_HASH is set, the content of one or more chunks does not match the hash in the CID
** @returns REQUEST_FAILED if one of the specified chunks has not been requested,
** @returns -1  if a socket error occurs. In that case errno is set with the appropriate code.
*/
int XgetChunkStatuses(int sockfd, ChunkStatus *statusList, int numCIDs) {
	int rc;
	char buffer[MAXBUFLEN];
	
	const char *buf = "CID list request status query";//Maybe send more useful information here.

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket\n", sockfd);
		return -1;
	}
	
	if (numCIDs == 0)
		return 0;

	if (!statusList) {
		LOG("statusList is null!");
		errno = EFAULT;
		return -1;
	}

	// protobuf message
	xia::XSocketMsg xsm;
	xsm.set_type(xia::XGETCHUNKSTATUS);

	xia::X_Getchunkstatus_Msg *x_getchunkstatus_msg = xsm.mutable_x_getchunkstatus();
  
	for (int i = 0; i < numCIDs; i++) {
		if (statusList[i].cid) {
			x_getchunkstatus_msg->add_dag(statusList[i].cid);
		} else {
			LOGF("cDAGv[%d] is NULL", i);
		}
	}

	if (x_getchunkstatus_msg->dag_size() == 0) {
		LOG("no dags were specified!");
		errno = EFAULT;
		return -1;
	}

	x_getchunkstatus_msg->set_payload((const char*)buf, strlen(buf) + 1);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}
     	 
	if ((rc = click_reply(sockfd, buffer, sizeof(buffer))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	xia::XSocketMsg xia_socket_msg1;
	xia_socket_msg1.ParseFromString(buffer);

	if (xia_socket_msg1.type() == xia::XGETCHUNKSTATUS) {
		
		xia::X_Getchunkstatus_Msg *x_getchunkstatus_msg1 = xia_socket_msg1.mutable_x_getchunkstatus();
		    
		char status_tmp[100];
		int status_for_all = READY_TO_READ;
		
		for (int i = 0; i < numCIDs; i++) {
			strcpy(status_tmp, x_getchunkstatus_msg1->status(i).c_str());
		    	
			if (strcmp(status_tmp, "WAITING") == 0) {
				statusList[i].status = WAITING_FOR_CHUNK;
				
				status_for_all &= ~READY_TO_READ;
				status_for_all |= WAITING_FOR_CHUNK;

			} else if (strcmp(status_tmp, "INVALID_HASH") == 0) {
				statusList[i].status = INVALID_HASH;

				status_for_all &= ~READY_TO_READ;
				status_for_all |= INVALID_HASH;
		    		
			} else if (strcmp(status_tmp, "READY") == 0) {
				statusList[i].status = READY_TO_READ;
		    	
			} else if (strcmp(status_tmp, "FAILED") == 0) {
				statusList[i].status = REQUEST_FAILED;

				status_for_all &= ~READY_TO_READ;
				status_for_all |= REQUEST_FAILED;
			
			} else {
				statusList[i].status = REQUEST_FAILED;
				status_for_all &= ~READY_TO_READ;
				status_for_all |= REQUEST_FAILED;
			}
		}
		    
		rc = status_for_all;
	} else
		rc = -1;
	
	return rc; 
}

/*!
** @file XreadChunk.c
** @brief implements XreadChunk()
*/

/*!
** @brief Reads the contents of the specified CID into rbuf. Must be called
** after XrequestChunk() or XrequestChunks().
**
** the CID specified in cid must be a full DAG, not a fragment such as is
** returned by XputChunk. For instance: "RE ( AD:AD0 HID:HID0 ) CID:<hash>"
** where <hash> is the 40 character hash of the content chunk generated 
** by the sender. The XputChunk() API call only returns <hash>. Either the 
** client or server application must generate the full DAG that is passed
** to this API call.
**
** @param sockfd the control socket (must be of type XSOCK_CHUNK)
** @param rbuf buffer to receive the data
** @param len length of rbuf
** @param flags currently unused
** @param cid the CID to retrieve. cid should be a full DAG, not a fragment.
** @param cidLen length of cid (currently unused)
**
** @returns number of bytes in the CID
** @returns -1 on error with errno set
**
*/
int XreadChunk(int sockfd, void *rbuf, size_t len, int /* flags */, char * cid, size_t /* cidLen */) {
	int rc;
	char UDPbuf[MAXBUFLEN];

	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket\n", sockfd);
		return -1;
	}

	if (len == 0)
		return 0;

	if (!rbuf || !cid) {
		LOG("null pointer error!");
		errno = EFAULT;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XREADCHUNK);

	xia::X_Readchunk_Msg *x_readchunk_msg = xsm.mutable_x_readchunk();
  
	x_readchunk_msg->set_dag(cid);

	std::string p_buf;
	xsm.SerializeToString(&p_buf);

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	}

	if ((rc = click_reply(sockfd, UDPbuf, sizeof(UDPbuf))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	std::string str(UDPbuf, rc);

	xsm.Clear();
	xsm.ParseFromString(str);

	xia::X_Readchunk_Msg *msg = xsm.mutable_x_readchunk();
	unsigned paylen = msg->payload().size();
	const char *payload = msg->payload().c_str();

	if (paylen > len) {
		LOGF("CID is %u bytes, but rbuf is only %lu bytes", paylen, len);
		errno = EFAULT;
		return -1;
	}

	memcpy(rbuf, payload, paylen);
	return paylen;
}

/*!
** @file XpushChunkto.c
** @brief implements XpushChunkto()
*/

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
		const struct sockaddr *addr, socklen_t addrlen, ChunkInfo *info) {
	
	int rc;
	  
	if ((rc = XputChunk(ctx, buf, len, info)) < 0)
		return rc;
	printf("CID put: %s\n", info->cid);


	/* if(ctx == NULL || buf == NULL || info == NULL || !addr) */
	if (!addr) {
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
		LOGF("truncating... requested size (%lu) is larger than XIA_MAXBUF (%d)\n", len, XIA_MAXBUF);
		len = XIA_MAXBUF;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XPUSHCHUNKTO);

	xia::X_Pushchunkto_Msg *x_pushchunkto = xsm.mutable_x_pushchunkto();

	// FIXME: validate addr
	Graph g((sockaddr_x*)addr);
	std::string s = g.dag_string();
	
	// printf("Destination DAG in API Call: %s \n", s.c_str());

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

	// because we don't have queueing or seperate control and data sockets, we 
	// can't get status back reliably on a datagram socket as multiple peers
	// could be talking to it at the same time and the control messages can get
	// mixed up with the data packets. So just assume that all went well and tell
	// the caller we sent the data

	return len;
}

int XpushBufferto(const ChunkContext *ctx, const char *data, size_t len, int flags,
		const struct sockaddr *addr, socklen_t addrlen, ChunkInfo **info, unsigned chunkSize) {
		
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
**/
int XpushFileto(const ChunkContext *ctx, const char *fname, int flags,
		const struct sockaddr *addr, socklen_t addrlen, ChunkInfo **info, unsigned chunkSize) {
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
** @file XrecvChunkfrom.c
** @brief implements XrecvChunkfrom()
*/

/*
 * recvfrom like datagram receiving function for XIA
 * does not fill in DAG fields yet
 */

/*!
** @brief receives datagram data on an Xsocket.
**
** XrecvChunkfrom() retrieves data from an Xsocket of type XSOCK_CHUNK. 
**
** XrecvChunkFrom() does not currently have a non-blocking mode, and will block
** until a data is available on sockfd. 
**
**
** @param sockfd The socket to receive with
** @param rbuf where to put the received data
** @param len maximum amount of data to receive. The amount of data
** returned may be less than len bytes.
** @param flags (This is not currently used but is kept to be compatible
** with the standard sendto socket call.
** @param ChunkInfo loads the received the chunkinfo
**
** @returns number of bytes received
** @returns -1 on failure with errno set.
*/


// FIXME: It's better if the whole source dag is returned. Have to modify push message and xiacontentmodule
int XrecvChunkfrom(int sockfd, void *rbuf, size_t len, int flags, ChunkInfo *ci) {
	int rc;
	char UDPbuf[MAXBUFLEN];


	if (flags != 0) {
		LOG("flags is not suppored at this time");
		errno = EINVAL;
		return -1;
	}

	if (!rbuf || !ci ) {
		LOG("null pointer!\n");
		errno = EFAULT;
		return -1;
	}	
	
	if (validateSocket(sockfd, XSOCK_CHUNK, EAFNOSUPPORT) < 0) {
		LOGF("Socket %d must be a chunk socket\n", sockfd);
		return -1;
	}

	if (len == 0)
		return 0;

	if (!rbuf || !ci) {
		LOG("null pointer error!");
		errno = EFAULT;
		return -1;
	}

	xia::XSocketMsg xsm;
	xsm.set_type(xia::XPUSHCHUNKTO);

	LOG("recvchunk API waiting for chunk ");
	if ((rc = click_reply(sockfd, UDPbuf, sizeof(UDPbuf))) < 0) {
		LOGF("Error retrieving status from Click: %s", strerror(errno));
		return -1;
	}

	std::string str(UDPbuf, rc);

	xsm.Clear();
	xsm.ParseFromString(str);
	
	xia::X_Pushchunkto_Msg *msg = xsm.mutable_x_pushchunkto();
	unsigned paylen = msg->payload().size();
	const char *payload = msg->payload().c_str();
	//FIXME: This fixes CID:cid to cid. The API should change to return the SDAG instead
 	strcpy(ci->cid, msg->cid().substr(4,msg->cid().npos).c_str());
	// strcpy(ci->cid, msg->cid().c_str());
	ci->size = msg->length();
	ci->timestamp.tv_sec=msg->timestamp();
	ci->timestamp.tv_usec = 0;
	ci->ttl = msg->ttl();
	

	if (paylen > len) {
		LOGF("CID is %u bytes, but rbuf is only %lu bytes", paylen, len);
		errno = EFAULT;
		return -1;
	}

	memcpy(rbuf, payload, paylen);
	return paylen;
}

/*!
** @file XputChunk.c
** @brief implements XputChunk(), XputFile(), XputBuffer(), XremoveChunk(), 
** XallocCacheSlice(),XfreeCacheSlice(), and XfreeChunkInfo()
*/

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
	if (sockfd < 0) {
		LOG("Unable to allocate the cache slice.\n");
		return NULL;
	} 
	else {
		// FIXME: contextID is going to need to be somethign else so we can have multiple ones
		// FIXME: add protobuf for this instead of rolling it up with the putChunk call
		ChunkContext *newCtx = (ChunkContext *)malloc(sizeof(ChunkContext));

		newCtx->contextID = getpid();
		newCtx->cachePolicy = policy;
		newCtx->cacheSize = size;
		newCtx->ttl = ttl;
		newCtx->sockfd = sockfd;
		// LOGF("New CTX: sock,policy,size=%d,%d,%d\n", sockfd, policy, size);
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
int XfreeCacheSlice(ChunkContext *ctx) {
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
int XputChunk(const ChunkContext *ctx, const char *data, unsigned length, ChunkInfo *info) {
	int rc;
	char buffer[MAXBUFLEN];

	if (length > XIA_MAXCHUNK) {
		errno = EMSGSIZE;
		LOGF("Chunk size of %d is too large\n", length);
		return -1;
	}

	if (ctx == NULL || data == NULL || info == NULL) {
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
	if ((rc = click_reply(ctx->sockfd, buffer, sizeof(buffer))) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

	xia::XSocketMsg _socketMsgReply;
	std::string bufStr(buffer, rc);
	_socketMsgReply.ParseFromString(bufStr);
	if (_socketMsgReply.type() == xia::XPUTCHUNK) {
		xia::X_Putchunk_Msg *_msgReply = _socketMsgReply.mutable_x_putchunk();
		info->size = _msgReply->payload().size();
		strcpy(info->cid, _msgReply->cid().c_str());
		info->ttl= _msgReply->ttl();
		info->timestamp.tv_sec=_msgReply->timestamp();
		info->timestamp.tv_usec = 0;
		LOGF(">>>>>> PUT: info->cid: %s \n", _msgReply->cid().c_str()); 
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
int XputFile(ChunkContext *ctx, const char *fname, unsigned chunkSize, ChunkInfo **info) {
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
int XputBuffer(ChunkContext *ctx, const char *data, unsigned len, unsigned chunkSize, ChunkInfo **info) {
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
int XremoveChunk(ChunkContext *ctx, const char *cid) {
	char buffer[2048];
	int rc;

	if (cid == NULL || ctx == NULL) {
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
	if ((rc = click_reply(ctx->sockfd, buffer, sizeof(buffer))) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

	xia::XSocketMsg _socketMsgReply;
	std::string bufStr(buffer, rc);
	_socketMsgReply.ParseFromString(bufStr);

	if (_socketMsgReply.type() == xia::XREMOVECHUNK) {
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
void XfreeChunkInfo(ChunkInfo *infop) {
	if (infop)
		free(infop);
}
