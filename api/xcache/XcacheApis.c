/* ts=4 */
/*
** Copyright 2017 Carnegie Mellon University
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
 @file XcacheApis.c
 @brief content specific APIs
*/

#include "xcache.h"
/*! \cond */
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "xcache_cmd.pb.h"
#include "irq.pb.h"
#include "xcache_sock.h"
#include "cid.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include "dagaddr.hpp"
/*! \endcond */

#define IO_BUF_SIZE (1024 * 1024)

static void (*notif_handlers[XCE_MAX])(XcacheHandle *, int, void *, size_t) = {
	NULL,
	NULL,
};

/** Helper functions **/
static int get_connected_socket(void)
{
	int sock;
	struct sockaddr_un xcache_addr;
	char sock_name[512];

	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if(sock < 0) {
		return -1;
	}

	if(get_xcache_sock_name(sock_name, 512) < 0) {
		close(sock);
		return -1;
	}

	/* Setup xcache's address */
	xcache_addr.sun_family = AF_UNIX;
	strcpy(xcache_addr.sun_path, sock_name);

	if(connect(sock, (struct sockaddr *)&xcache_addr, sizeof(xcache_addr)) < 0) {
		printf("%s:%d error:%s\n", __FILE__, __LINE__, strerror(errno));
		close(sock);
		return -1;
	}

	return sock;
}

static int send_command(int xcache_sock, xcache_cmd *cmd)
{
	int ret;
	int remaining, sent;
	uint32_t msg_length;
	std::string cmd_on_wire;

	cmd->SerializeToString(&cmd_on_wire);

	remaining = cmd_on_wire.length();

	msg_length = htonl(remaining);
	send(xcache_sock, &msg_length, 4, 0);

	sent = 0;
	do {
		ret = send(xcache_sock, cmd_on_wire.c_str() + sent, remaining, 0);
		if (ret <= 0) {
			break;
		}
		remaining -= ret;
		sent += ret;
	} while(remaining > 0);

	//fprintf(stderr, "%s: Lib sent %d bytes\n", __func__, htonl(msg_length) + 4);

	if (ret < 0 || remaining > 0)
		return -1;
	else
		return htonl(msg_length) + 4;
}

static int read_bytes_to_buffer(int fd, std::string &buffer, int remaining)
{
	int ret;
	char *buf = (char *)malloc(remaining);
	int total = remaining;
	char *p = buf;

	while(remaining > 0) {

		ret = read(fd, p, remaining);
		if(ret <= 0)
			return ret;
		p += ret;
		remaining -= ret;
	}

	buffer.assign(buf, total);
	free(buf);

	return 1;
}

static int get_response_blocking(int xcache_sock, xcache_cmd *cmd)
{
	std::string buffer;
	int ret;
	uint32_t msg_length, remaining;

	if (read(xcache_sock, &msg_length, 4) != 4) {
		fprintf(stderr, "%s: Error\n", __func__);
		return -1;
	}

	remaining = ntohl(msg_length);
	//fprintf(stderr, "Lib received msg of length %d\n", remaining);
	ret = read_bytes_to_buffer(xcache_sock, buffer, remaining);

	if (ret == 0) {
		cmd->set_cmd(xcache_cmd::XCACHE_ERROR);
		return -1;
	}
	cmd->ParseFromString(buffer);

	return 0;
}

/*!
** @brief initialize an Xcache buffer
**
** Initializes the fields in an XcacheBuf to default values.
**
** @note This does not currently reset a previously allocated XcacheBuf.
**
** @param xbuf pointer to a new XcacheBuf to intialize
**
** @returns 0 on success
** @returns -1 if xbuf is NULL
*/
int XbufInit(XcacheBuf *xbuf)
{
	if (xbuf != NULL) {
		xbuf->length = 0;
		xbuf->buf = NULL;

		return 0;
	}

	return -1;
}

/*!
** @brief appends data to an XcacheBuf
**
** Reallocates the storage inside the Xcachebuf and appends data to it.
**
** @param xbuf pointer to a new XcacheBuf to intialize
**
** @returns 0 on success
** @returns -1 on error with xbuf unchanged
*/
int XbufAdd(XcacheBuf *xbuf, void *data, size_t len)
{
	if (xbuf == NULL) {
		return -1;
	} else if (data == NULL || len == 0) {
		return 0;
	}

	void *p = realloc(xbuf->buf, xbuf->length + len);

	if (p != NULL) {
		xbuf->buf = p;
		memcpy((char *)xbuf->buf + xbuf->length, data, len);
		xbuf->length += len;

		return 0;
	}

	return -1;
}

/*!
** @brief frees the contents of an XcacheBuf
**
** Frees the contents of an XcacheBuf.
**
** @param xbuf pointer to a XcacheBuf to free
**
*/
void XbufFree(XcacheBuf *xbuf)
{
	if (xbuf) {
		if (xbuf->buf) {
			free(xbuf->buf);
		}
		xbuf->length = 0;
		xbuf->buf = NULL;
	}
}

/*!
** @brief destroy an XcacheHandle
**
** Closes the sockets associated with h and clears internal state.
**
** @param h handle to destroy
**
*/
int XcacheHandleDestroy(XcacheHandle *h)
{
	xcache_cmd cmd;

	cmd.set_cmd(xcache_cmd::XCACHE_FREE_CONTEXT);
	cmd.set_context_id(h->contextID);
	send_command(h->xcacheSock, &cmd);

	close(h->xcacheSock);
	close(h->notifSock);

	h->xcacheSock = -1;
	h->notifSock = -1;
	h->contextID = 0;
	h->ttl = 0;

	return 0;
}

/*!
** @brief initialize an XcacheHandle
**
** Prepare an XcacheHandle for use.
**
** @param h handle to initialize
**
** @returns 0 on success
** @returns -1 on error
**
*/
int XcacheHandleInit(XcacheHandle *h)
{
	xcache_cmd cmd;

	if (!h) {
		return -1;
	}

	h->ttl = 0;
	h->xcacheSock = get_connected_socket();

	if (h->xcacheSock < 0) {
		return -1;
	}

	h->notifSock = get_connected_socket();

	if (h->notifSock < 0) {
		close(h->xcacheSock);
		return -1;
	}

	cmd.set_cmd(xcache_cmd::XCACHE_ALLOC_CONTEXT);
	send_command(h->xcacheSock, &cmd);

	if (get_response_blocking(h->xcacheSock, &cmd) >= 0) {
		fprintf(stderr, "Msg type = %d\n", cmd.cmd());
		fprintf(stderr, "Library received context id = %d\n", cmd.context_id());

		h->contextID = cmd.context_id();
	} else {
		fprintf(stderr, "Library get_response_blocking failed\n");
	}

	cmd.set_cmd(xcache_cmd::XCACHE_FLAG_DATASOCKET);
	cmd.set_context_id(h->contextID);
	send_command(h->xcacheSock, &cmd);

	cmd.set_cmd(xcache_cmd::XCACHE_FLAG_NOTIFSOCK);
	cmd.set_context_id(h->contextID);
	send_command(h->notifSock, &cmd);

	return 0;
}


/*!
** @brief set TTL for newly created chunks
**
** Sets the Time To Live (TTL) in seconds for all chunks created with this XcacheHandle. This
** only applies to chunks created after the TTL has been set.
** The default value (0) is to live forever.
**
** @param h XcacheHandle to modify
** @param ttl time to live in seconds.
**
** @returns 0 on success
** @returns -1 on error
**
*/
int XcacheHandleSetTtl(XcacheHandle *h, time_t ttl)
{
	int rc = -1;

	if (h && ttl >= 0) {
		h->ttl = ttl;
		rc = 0;
	}
	return rc;
}

/*!
** @brief evict a chunk from the local content cache
**
** Evicts a chunk from the local content cache.
**
** @note this only applies to the system the command is run on. It does not affect any other hosts
** or routers where this chunk may be cached.
**
** @param h cache handle
** @param CID of the chunk to evict Either the full CID (CID:nnnnnn) or just the hash (nnnnnn) is valid.
**
** @returns XCACHE_OK on success
** @returns XCACHE_INVALID_CID if cid is not properly formed
** @returns -1 if a communication error with the xcache daemon occurs
**
*/
int XevictChunk(XcacheHandle *h, const char *cid)
{
	int rc = xcache_cmd::XCACHE_OK;
	xcache_cmd cmd;
	int id_offset = 0;

	if (strncasecmp(cid, "cid:", 4) == 0) {
		id_offset = strlen("cid:");
	}
	if (strncasecmp(cid, "ncid:", 4) == 0) {
		id_offset = strlen("ncid:");
	}
	if (strlen(cid+id_offset) != (XID_SIZE * 2)) {
		return xcache_cmd::XCACHE_INVALID_CID;
	}

	cmd.set_cmd(xcache_cmd::XCACHE_EVICT);
	cmd.set_context_id(h->contextID);
	cmd.set_cid(cid);
	printf("evict sending\n");

	if(send_command(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "%s: Error in sending command to xcache\n", __func__);
		/* Error in Sending chunk */
		return -1;
	}

	if(get_response_blocking(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Did not get a valid response from xcache\n");
		return -1;
	}

	return rc;
}

static int __XputChunk(XcacheHandle *h, const char *data, size_t length,
		sockaddr_x *addr, int flags)
{
	xcache_cmd cmd;

	cmd.set_cmd(xcache_cmd::XCACHE_STORE);
	cmd.set_context_id(h->contextID);
	cmd.set_data(data, length);
	cmd.set_flags(flags);
	cmd.set_ttl(h->ttl);

	if(send_command(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "%s: Error in sending command to xcache\n", __func__);
		/* Error in Sending chunk */
		return -1;
	}

	if(get_response_blocking(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Did not get a valid response from xcache\n");
		return -1;
	}

	if(cmd.cmd() == xcache_cmd::XCACHE_ERROR) {
		printf("%s received an error from xcache\n", __func__);
		if(cmd.status() == xcache_cmd::XCACHE_ERR_EXISTS) {
			fprintf(stderr, "%s: Error this chunk already exists\n", __func__);
			return xcache_cmd::XCACHE_ERR_EXISTS;
		}
	}

	//fprintf(stderr, "%s: Got a response from server\n", __func__);
	memcpy(addr, cmd.dag().c_str(), cmd.dag().length());

	//Graph g(addr);
	//g.print_graph();

	return xcache_cmd::XCACHE_OK;
}

static inline int __XputDataChunk(XcacheHandle *h, const char *data,
		size_t length, sockaddr_x *addr)
{
	return __XputChunk(h, data, length, addr, XCF_DATACHUNK);
}

static inline int __XputMetaChunk(XcacheHandle *h, const char *data, size_t length, sockaddr_x *addr)
{
	return __XputChunk(h, data, length, addr, XCF_METACHUNK);
}

/*!
** @brief load a chunk into the local content cache
**
** Creates a CID based on the hash of the content supplied in data, caches the chunk locally, and
** returns a DAG in the form of "RE (AD HID) CID" that refers to the local address of the newly created chunk.
** A route to the CID is added to the node's routing table.
** The chunk will expire out of the local cache (and any other locations where it is subsequently
** cached) if a TTL has been set in in the XcacheHandle.
**
** @param h the cache handle
** @param data a byte array of data to be converted to a chunk
** @param length the number of bytes in data
** @param addr sockaddr_x to receive the DAG that points to the new chunk
**
** @returns XCACHE_OK on success
** @returns XCACHE_ERR_EXISTS if the chunk already resides in the cache
** @returns -1 if a communication error with the xcache daemon occurs
**
*/
int XputChunk(XcacheHandle *h, const char *data, size_t length,
		sockaddr_x *addr)
{
	return __XputDataChunk(h, data, length, addr);
}

/*!
** @brief load a named chunk into the local content cache
**
** Creates an NCID based on hash of Content URI and Publisher public key
**
** Note that each NCID chunk also has a CID address.
**
** The chunk is stored in the local content cache and is accessible via
** both NCID and CID. Both of those identifiers are entered into the
** corresponding routing tables on the local host.
**
** The chunk will expire out of the local cache (and any other locations
** where it is subsequently cached) if a TTL has been set in XcacheHandle.
**
** @param h the cache handle
** @param data a byte array of data to be converted to a chunk
** @param length the number of bytes in data
** @param publisher_name name of publisher signing this chunk
**
** @returns XCACHE_OK on success
** @returns XCACHE_ERR_EXISTS if the chunk already resides in the cache
** @returns -1 if an error occurs in producing or signing the chunk
**
*/
int XputNamedChunk(XcacheHandle *h, const char *data, size_t length,
		const char *content_name, const char *publisher_name)
{
	// Build and forward an NCID_STORE request to Xcache controller
	xcache_cmd cmd;

	cmd.set_cmd(xcache_cmd::XCACHE_STORE_NAMED);
	cmd.set_context_id(h->contextID);
	cmd.set_data(data, length);
	// TODO: limit size of content_name and publisher_name
	cmd.set_content_name(content_name, strlen(content_name));
	cmd.set_publisher_name(publisher_name, strlen(publisher_name));
	cmd.set_ttl(h->ttl);

	if(send_command(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "%s: Error in sending command to xcache\n", __func__);
		/* Error in Sending chunk */
		return -1;
	}

	if(get_response_blocking(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Did not get a valid response from xcache\n");
		return -1;
	}

	if(cmd.cmd() == xcache_cmd::XCACHE_ERROR) {
		printf("%s received an error from xcache\n", __func__);
		if(cmd.status() == xcache_cmd::XCACHE_ERR_EXISTS) {
			fprintf(stderr, "%s: Error this chunk already exists\n", __func__);
			return xcache_cmd::XCACHE_ERR_EXISTS;
		} else {
			return -1;
		}
	}

	//fprintf(stderr, "%s: Got a response from server\n", __func__);

	return xcache_cmd::XCACHE_OK;
}

/*!
** @brief create and cache a chunk consisting of addresses of other chunks
**
** Creates a content chunk that contain a list of DAGs pointing to other CIDs The meta chunk is
** cached locally, and a DAG in the form of "RE (AD HID) CID" that refers to the local address of
** the meta chunk is returned.
** A route to the meta chunk's CID is added to the node's routing table.
** The meta chunk will expire out of the local cache (and any other locations where it is subsequently
** cached) if a TTL has been set in in the XcacheHandle.
**
** @note This function should be modified to condense the space required for each sockaddr_x otherwise
** the chunk will consist of a large amount of unused space.
**
** @param h the cache handle
** @param metachunk pointer to a sockaddr_x that will recieve the address of the new meta chunk
** @param addrs an array of DAGs (most like=ly for CIDs) to be stored in the meta chunk
** @param addrlen the size of a sockaddr_x (FIXME: why is this needed?)
** @param count the number of addrs to add to the meta chunk
**
** @returns XCACHE_OK on success
** @returns XCACHE_ERR_EXISTS if the chunk already resides in the cache
** @returns -1 if a communication error with the xcache daemon occurs
**
*/
int XputMetaChunk(XcacheHandle *h, sockaddr_x *metachunk, sockaddr_x *addrs, socklen_t addrlen, int count)
{
	sockaddr_x *data = (sockaddr_x *)calloc(count, sizeof(sockaddr_x));
	xcache_cmd cmd;
	int i;

	if(!data)
		return -1;

	for(i = 0; i < count; i++) {
		memcpy(&data[i], &addrs[i], addrlen);
	}

	if (__XputMetaChunk(h, (char *)data, sizeof(sockaddr_x) * count, metachunk) < 0) {
		free(data);
		return -1;
	}

	free(data);

	return 0;
}

/*!
** @brief breaks a file into a series of chunks
**
** Chunks the file fname into one or more chunks each containing up to chunkSize bytes.
** A block of memory large enough to hold the list of new DAGs is created and should be freed by
** the calling code when it is done with the addresses. Each DAG is in the form of
** "RE (AD HID) CID".
** A route for each chunk's CID is added to the node's routing table.
** The chunks will expire out of the local cache (and any other locations where it is subsequently
** cached) if a TTL has been set in in the XcacheHandle.
**
** @param h the cache handle
** @param fname the file to cache
** @param chunkSize the maximum size for each chunk. If chunksize is 0, the default size of 1mb
**  per chunk will be used.
** @param addrs pointer to a variable which will receive the array of DAGs that refer to the newly
**  created chunks. The memory pointered to by addr should be freed by the calling code.
**
** @returns the number of chunks created
** @returns -1 with errno set appropriately if a file error occurs
**
*/
int XputFile(XcacheHandle *h, const char *fname, size_t chunkSize, sockaddr_x **addrs)
{
	FILE *fp;
	struct stat fs;
	sockaddr_x *addrlist;
	unsigned numChunks;
	unsigned i;
	int rc;
	int count;
	char *buf;

	if (h == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (fname == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (chunkSize == 0)
		chunkSize =  DEFAULT_CHUNK_SIZE;

	if (stat(fname, &fs) != 0)
		return -1;

	if (!(fp = fopen(fname, "rb")))
		return -1;

	numChunks = fs.st_size / chunkSize;
	if (fs.st_size % chunkSize)
		numChunks ++;

	if (!(addrlist = (sockaddr_x *)calloc(numChunks, sizeof(sockaddr_x)))) {
		fclose(fp);
		return -1;
	}

	if (!(buf = (char*)malloc(chunkSize))) {
		free(addrlist);
		fclose(fp);
		return -1;
	}

	i = 0;
	while (!feof(fp)) {
		if ((count = fread(buf, sizeof(char), chunkSize, fp)) > 0) {
			rc = XputChunk(h, buf, count, &addrlist[i]);
			if(rc < 0) {
				printf("Xputchunk failed in XputFile\n");
				break;
			}
			if(rc == xcache_cmd::XCACHE_ERR_EXISTS) {
				// TODO: does this cause us to not add the chunk to the list of what we send to the other end???
				printf("chunk already exists in the cache\n");
				continue;
			}
			i++;
		}
	}

	rc = i;

	*addrs = addrlist;
	fclose(fp);
	free(buf);

	return rc;
}

/*!
** @brief breaks a large buffer into a series of chunks
**
** Chunks the buffer into one or more chunks each containing up to chunkSize bytes.
** A block of memory large enough to hold the list of new DAGs is created and should be freed by
** the calling code when it is done with the addresses. Each DAG is in the form of
** "RE (AD HID) CID".
** A route for each chunk's CID is added to the node's routing table.
** The chunks will expire out of the local cache (and any other locations where it is subsequently
** cached) if a TTL has been set in in the XcacheHandle.
**
** @param h the cache handle
** @param data the buffer to cache
** @param the length of data
** @param chunkSize the maximum size for each chunk. If chunksize is 0, the default size of 1mb
**  per chunk will be used.
** @param addrs pointer to a variable which will receive the array of DAGs that refer to the newly
**  created chunks. The memory pointered to by addr should be freed by the calling code.
**
** @returns the number of chunks created
** @returns -1 with errno set appropriately if an error occurs
**
*/
int XputBuffer(XcacheHandle *h, const char *data, size_t length, size_t chunkSize, sockaddr_x **addrs)
{
	sockaddr_x *addrlist;
	unsigned numChunks;
	unsigned i;
	int rc;
	char *buf;
	unsigned offset;

	if(h == NULL) {
		errno = EFAULT;
		return -1;
	}

	if(chunkSize == 0)
		chunkSize =  DEFAULT_CHUNK_SIZE;
	else if(chunkSize > XIA_MAXBUF)
		chunkSize = XIA_MAXBUF;

	numChunks = length / chunkSize;
	if(length % chunkSize)
		numChunks ++;

	if(!(addrlist = (sockaddr_x *)calloc(numChunks, sizeof(sockaddr_x)))) {
		return -1;
	}

	if(!(buf = (char*)malloc(chunkSize))) {
		free(addrlist);
		return -1;
	}

	i = 0;
	offset = 0;
#ifndef MIN
#define MIN(__x, __y) ((__x) < (__y) ? (__x) : (__y))
#endif
	while(offset < length) {
		int to_copy = MIN(length - offset, chunkSize);
		memcpy(buf, data + offset, to_copy);
		rc = XputChunk(h, buf, to_copy, &addrlist[i]);
		if(rc < 0)
			break;
		if(rc == xcache_cmd::XCACHE_ERR_EXISTS) {
			continue;
		}
		offset += to_copy;
		i++;
	}

	rc = i;

	*addrs = addrlist;
	free(buf);

	return rc;
}

/*!
** @brief breaks the contents of an XcacheBuf into chunks
**
** Chunks the contents of xbuf into one or more chunks each containing up to chunkSize bytes.
**
** Calls XputBuffer() internally.
**
** @param h the cache handle
** @param xbuf the XcachedBuf to be cached
** @param chunkSize the maximum size for each chunk. If chunksize is 0, the default size of 1mb
**  per chunk will be used.
** @param addrs pointer to a variable which will receive the array of DAGs that refer to the newly
**  created chunks. The memory pointered to by addr should be freed by the calling code.
**
** @returns the number of chunks created
** @returns -1 with errno set appropriately if an error occurs
**
*/
inline int XbufPut(XcacheHandle *h, XcacheBuf *xbuf, size_t chunkSize, sockaddr_x **addrs)
{
	return XputBuffer(h, xbuf->buf, xbuf->length, chunkSize, addrs);
}

/*!
 * @brief fetch a chunk by its name
 *
 * Fetches the specified chunk from the network. The caller provides a
 * name for the chunk that it can derive based on an agreement/protocol
 * with the publisher. The name could be completely arbitrary, but for
 * now we are using <Publisher_name>/<Content_name_and_attrs>, e.g.
 *    Netflix/Movies/2016/Moana/useragent=Safari/ver=10.8...
 *    where, Publisher_name = Netflix
 *    content_name_and_attrs = Movies/2016/Moana/useragent=Safari/ver=10.8...
 *
 * @param h the xcache handle
 * @param buf a pointer to the chunk's data. Caller must free data
 * @param flags user specified flags for content retrieval characteristics
 * @param name string representing content to be retrieved
 *
 * @returns size of data stored in *buf
 * @return -1 on error
 */
int XfetchNamedChunk(XcacheHandle *h, void **buf, int flags, const char *name)
{
	xcache_cmd cmd;
	cmd.set_cmd(xcache_cmd::XCACHE_FETCHNAMEDCHUNK);
	cmd.set_context_id(h->contextID);
	cmd.set_content_name(name);
	cmd.set_flags(flags);

	if(send_command(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Error in sending command to xcache\n");
		/* Error in Sending chunk */
		return -1;
	}
	fprintf(stderr, "Command sent to xcache successfully\n");

	if(flags & XCF_BLOCK) {
		size_t to_copy;

		if (get_response_blocking(h->xcacheSock, &cmd) < 0) {
			fprintf(stderr, "Did not get a valid response from xcache\n");
			return -1;
		}

		if (cmd.cmd() == xcache_cmd::XCACHE_ERROR) {
			fprintf(stderr, "Chunk fetch failed\n");
			return -1;
		}

		to_copy = cmd.data().length();
		*buf = malloc(to_copy);
		memcpy(*buf, cmd.data().c_str(), to_copy);

		return to_copy;
	}
	return 0;
}

/*!
 * @brief check if a chunk is cached in the local cache
 *
 * Requests the local Xcache to check if the requested chunk is in the
 * local cache.
 *
 * @returns 1 if the chunk in local
 * @returns 0 if the chunk was not found in local cache
 * @returns -1 if there was an error
 */
int XisChunkLocal(XcacheHandle *h, const char *chunk)
{
	// Send a request to Xcache to see if the chunk is local
	xcache_cmd cmd;
	cmd.set_cmd(xcache_cmd::XCACHE_ISLOCAL);
	cmd.set_cid(chunk);
	if(send_command(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Error sending isLocal command to Xcache\n");
		return -1;
	}

	// Synchronously wait for Xcache response
	if(get_response_blocking(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Invalid isLocal response from Xcache\n");
		return -1;
	}

	// Return 1 only if xcache explicitly told us that the chunk is local
	if(cmd.cmd() == xcache_cmd::XCACHE_ISLOCAL
			&& cmd.status() == xcache_cmd::XCACHE_OK) {
		return 1;
	}

	// The chunk was not local
	return 0;
}

/*!
** @brief fetch a chunk from the network
**
** Fetches the specified chunk from the network. The chunk is retrieved from the origin server specified
** in addr unless it is found in the local cache or on one of the in-path routers between the client
** and server. Because the CID of a chunk is the hash of the content, a chunk can safely be returned
** from an intermediate router that is closer to the client than the origin server.
**
** @note non-blocking fetches have not been extensively tested and may not work as expected
**
** @param h the cache handle
** @param buf a variable to recieve a pointer to the chunk's data. The value in buf should be freed
**  when the caller is through with it.
** @param flags bitmap of options to use when retrieving the chunk
**  \n XCF_BLOCK (RECOMMENDED TO BE SET) block until the entire chunk has arrived then return it to the caller
**  \n XCF_DISABLENOTIF (not currently used)
**  \n XCF_CACHE if set cache the chunk locally as well as return it to the caller
** @param addr the DAG of the chunk to retreive
** @param len the size of the memory pointed to by addr (should be sizeof(sockaddr_x))
**
** @returns size of data stored in *buf
** @returns -1 with errno set appropriately if an error occurs
**
*/
int XfetchChunk(XcacheHandle *h, void **buf, int flags, sockaddr_x *addr, socklen_t len)
{
	xcache_cmd cmd;

	fprintf(stderr, "Inside %s\n", __func__);

	// Bypass cache if blocked requesting chunk without caching
	/*
	if ( !(flags & XCF_CACHE) && (flags & XCF_BLOCK)) {
		return _XfetchRemoteChunkBlocking(buf, addr, len);
	}
	*/

	cmd.set_cmd(xcache_cmd::XCACHE_FETCHCHUNK);
	cmd.set_context_id(h->contextID);
	cmd.set_dag(addr, len);
	cmd.set_flags(flags);

	if(send_command(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Error in sending command to xcache\n");
		/* Error in Sending chunk */
		return -1;
	}
	fprintf(stderr, "Command sent to xcache successfully\n");

	if(flags & XCF_BLOCK) {
		size_t to_copy;

		if (get_response_blocking(h->xcacheSock, &cmd) < 0) {
			fprintf(stderr, "Did not get a valid response from xcache\n");
			return -1;
		}

		if (cmd.cmd() == xcache_cmd::XCACHE_ERROR) {
			fprintf(stderr, "Chunk fetch failed\n");
			return -1;
		}

		to_copy = cmd.data().length();
		*buf = malloc(to_copy);
		memcpy(*buf, cmd.data().c_str(), to_copy);

		return to_copy;
	}

	return 0;
}

/*!
 * @brief bypass xcache and fetch chunk without caching
 *
 * This function is an optimization for fetching chunks faster. It achieves
 * the speed by bypassing requesting chunks through xcache daemon and
 * caching them on the localhost.
 *
 * Note: If called on a router, the xcache daemon will still passively
 * be able to cache the chunks and may affect performance.
 *
 * @param buf pointer to chunk's data. Caller must free it
 * @param addr the DAG of the chunk to retrieve
 * @param len size of addr. Should be sizeof(sockaddr_x)
 *
 * @returns size of data stored in *chunk
 * @returns -1 on failure
 */
int _XfetchRemoteChunkBlocking(void **chunk, sockaddr_x *addr, socklen_t len)
{
	// NOTE: Code copied from xcache_controller::fetch_content_remote()
	// Must stay in sync with that code base
	int sock;
	Graph g(addr);

	fprintf(stderr, "Fetching: %s\n", g.dag_string().c_str());

	sock = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if (sock < 0) {
		fprintf(stderr, "ERROR creating Xsocket: %s\n", strerror(errno));
		return -1;
	}

	if (Xconnect(sock, (struct sockaddr *)addr, len) < 0) {
		fprintf(stderr, "ERROR connecting to CID: %s\n", strerror(errno));
		return -1;
	}

	std::string data;
	char buf[IO_BUF_SIZE];
	struct cid_header header;
	int to_recv, recvd;
	size_t remaining;
	size_t offset;

	Node expected_cid(g.get_final_intent());

	remaining = sizeof(cid_header);
	offset = 0;

	while (remaining > 0) {
		fprintf(stderr, "Remaining(1) = %lu\n", remaining);
		recvd = Xrecv(sock, (char *)&header + offset, remaining, 0);
		if (recvd < 0) {
			fprintf(stderr, "Sender Closed the connection: %s", strerror(errno));
			Xclose(sock);
			assert(0);
			break;
		} else if (recvd == 0) {
			fprintf(stderr, "Xrecv returned 0\n");
			break;
		}
		remaining -= recvd;
		offset += recvd;
	}

	remaining = ntohl(header.length);

	while (remaining > 0) {
		to_recv = remaining > IO_BUF_SIZE ? IO_BUF_SIZE : remaining;

		recvd = Xrecv(sock, buf, to_recv, 0);
		if (recvd < 0) {
			fprintf(stderr, "Receiver Closed the connection; %s", strerror(errno));
			Xclose(sock);
			assert(0);
			break;
		} else if (recvd == 0) {
			fprintf(stderr, "Xrecv returned 0");
			break;
		}
		fprintf(stderr, "recvd = %d, to_recv = %d\n", recvd, to_recv);

		remaining -= recvd;
		std::string temp(buf, recvd);

		data += temp;
	}

	// TODO: Verify that data matches intent CID in addr

	// Copy data to a buffer to be returned
	int to_copy = data.length();
	*chunk = malloc(to_copy);
	bzero(*chunk, to_copy);
	memcpy(*chunk, data.c_str(), to_copy);

	Xclose(sock);

	return to_copy;
}

/* Notficiations */
/*!
** @brief register to receive notifications when chunks arrive
**
** Start a thread to receive notifications when chunks requested using the XcacheHandle h arrive.
**
** @note this API is not currently working correctly,
**
** @param h the cache handle used when XfetchChunk was called
** @param event the type of event to return notificatioins for.
** @param data the data to be passed to the callback function
** @param datalen the length of data
**
** @returns 0 on success
** @returns an error code on failure (see the man page for pthread_create() for more details)
**
*/
int XregisterNotif(int event, void (*func)(XcacheHandle *, int event, void *data, size_t datalen))
{
	if(event < XCE_MAX) {
		notif_handlers[event] = func;
		return 0;
	}
	return -1;
}

static void *__notifThread(void *arg)
{
	XcacheHandle *h = (XcacheHandle *)arg;
	uint32_t msg_length;
	int ret;
	xcache_notif notif;

	do {
		std::cout << "Waiting for Notifications .... \n";
		ret = recv(h->notifSock, &msg_length, 4, 0);
		if(ret <= 0) {
			std::cout << "Notif Thread Killed.\n";
			pthread_exit(NULL);
		}

		std::cout << "[LIB] Received notification\n";

		msg_length = ntohl(msg_length);
		std::string buffer("");

		ret = read_bytes_to_buffer(h->notifSock, buffer, msg_length);
		if(ret == 0) {
			std::cout << "Error while receiving.\n";
			continue;
		}
		// TODO: The application should just take xcache_notif protobuf
		// For now, we call different handlers with different data
		// based on the type of notification
		notif.ParseFromString(buffer);

		// Notify application of a chunk that is now cached
		if(notif.has_arrived()) {
			notif_handlers[notif.cmd()](h, notif.cmd(),
					(void *)notif.arrived().dag().c_str(),
					notif.arrived().dag().length());
		}

		// Notify application with entire chunk contents as accepted
		// and verified by PushProxy in Xcache
		if(notif.has_contents()) {
			std::string buf;
			notif.contents().SerializeToString(&buf);
			notif_handlers[notif.cmd()](h, notif.cmd(),
					(void *)buf.c_str(), buf.length());
		}

	} while(1);
}

/*!
** @brief fetch a partial chunk
**
** Start a thread to receive notifications when chunks requested using the XcacheHandle h arrive.
**
** @note this API is not currently working correctly,
**
** @param h the cache handle used when XfetchChunk was called
**
** @returns 0 on success
** @returns an error code on failure (see the man page for pthread_create() for more details)
**
*/
int XlaunchNotifThread(XcacheHandle *h)
{
	pthread_t thread;

	return pthread_create(&thread, NULL, __notifThread, (void *)h);
}

/*!
** @brief a proxy for receiving pushed chunks
**
** Start a proxy process that can receive chunks pushed from remote
** Xcache instances. The user may start any number of proxies for
** various publishers or groups of publishers. Eventually we may allow
** different caching policies for each proxy.
**
** @param h the xcache handle
** @param proxyaddr address of newly created proxy returned to user
**
** @returns 0 on success and proxyaddr has proxy address
** @returns -1 on failure
*/
int XnewProxy(XcacheHandle *h, std::string &proxyaddr)
{
	// Request Xcache to start a new Proxy for pushed content
	xcache_cmd cmd;

	cmd.set_cmd(xcache_cmd::XCACHE_NEWPROXY);
	cmd.set_context_id(h->contextID);

	if(send_command(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Error starting new proxy\n");
		return -1;
	}

	if(get_response_blocking(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "No valid response to new proxy creation\n");
		return -1;
	}

	if(cmd.cmd() != xcache_cmd::XCACHE_NEWPROXY
			|| cmd.status() != xcache_cmd::XCACHE_OK) {
		fprintf(stderr, "Failed starting proxy\n");
		return -1;
	}

	printf("XnewProxy: started at: %s\n", cmd.dag().c_str());

	// The command was successful, get the dag for proxy
	proxyaddr.assign(cmd.dag());

	return 0;
}


int XrequestPushedChunk(std::string &chunkaddr,
		std::string &fetchservice, std::string &returnaddr)
{
	int sockfd;
	int state = 0;
	sockaddr_x fs_addr;
	Graph *g;
	InterestRequest irq;
	std::string irqstr;
	uint32_t irqstrlen, nirqstrlen;
	int remaining, offset;
	char *buf;
	int sent;
	int retval = -1;

	// Build the client request to be sent to fetchservice
	irq.set_chunk_addr(chunkaddr);
	irq.set_return_addr(returnaddr);
	irq.SerializeToString(&irqstr);

	// Connect to the fetchservice
	sockfd = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if(sockfd < 0) {
		fprintf(stderr, "Error creating sock to talk to FS\n");
		goto request_pushed_chunk_done;
	}
	g = new Graph(fetchservice);
	g->fill_sockaddr(&fs_addr);
	if(Xconnect(sockfd, (sockaddr *)&fs_addr, sizeof(sockaddr_x))) {
		fprintf(stderr, "Error connecting to FS\n");
		goto request_pushed_chunk_done;
	}
	state = 1;	// close sockfd

	// Send length of the interest request to fetchservice
	irqstrlen = irqstr.size();
	nirqstrlen = htonl(irqstrlen);	// Network byte order
	if(Xsend(sockfd, &nirqstrlen, sizeof(nirqstrlen),0)
			!= sizeof(nirqstrlen)) {
		fprintf(stderr, "Error sending interest request size\n");
		goto request_pushed_chunk_done;
	}

	// Now send the serialized irqstr to the fetchservice
	buf = (char *) malloc(irqstrlen);
	if(buf == NULL) {
		fprintf(stderr, "Error allocating memory for interest request\n");
		goto request_pushed_chunk_done;
	}
	state = 2;	// free buf
	bzero(buf, irqstrlen);
	memcpy(buf, irqstr.c_str(), irqstrlen);

	remaining = irqstrlen;
	offset = 0;
	while(remaining) {
		sent = Xsend(sockfd, &buf[offset], remaining, 0);
		if(sent < 0) {
			fprintf(stderr, "Error sending interest request\n");
			goto request_pushed_chunk_done;
		}
		remaining -= sent;
		offset += sent;
	}
	if(remaining != 0) {
		fprintf(stderr, "Error sending entire request\n");
		goto request_pushed_chunk_done;
	}
	retval = 0;

request_pushed_chunk_done:
	switch(state) {
		case 2: free(buf);
		case 1: Xclose(sockfd);
	};
	return retval;
}

/*!
** @brief push a chunk to the requested address
**
** Send a chunk to a remote service. The user provides the address of
** the chunk to be sent and the recipient service address.
** Typically, the recipient service should know how to handle the
** incoming chunk header and contents.
**
** @param h The cache handle
** @param chunk DAG of the chunk to be sent. Must be local
** @param addr Recipient address
**
** @returns 0 on success
** @returns -1 on failure
*/
int XpushChunk(XcacheHandle *h, sockaddr_x *chunk, sockaddr_x *addr)
{
	xcache_cmd cmd;

	cmd.set_cmd(xcache_cmd::XCACHE_PUSHCHUNK);
	cmd.set_data(chunk, sizeof(sockaddr_x));
	cmd.set_dag(addr, sizeof(sockaddr_x));
	if(send_command(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Error pushing chunk to remote address\n");
		return -1;
	}
	if(get_response_blocking(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "No valid response to chunk push\n");
		return -1;
	}
	// TODO: read the response code and return error, when necessary
	return 0;
}
/*!
** @brief fetch a partial chunk
**
** Depricated, use XfetchChunk() instead.
**
** @param h the cache handle
** @param addr the DAG of the chunk to retreive
** @param addrlen the size of the memory pointed to by addr (should be sizeof(sockaddr_x))
** @param buf location to copy the chunk data
** @param buflen size of buf
** @param offset location inside of the chunk to copy from
**
** @returns # of bytes returned in buf
** @returns -1 with errno set appropriately if an error occurs
**
*/
int XreadChunk(XcacheHandle *h, sockaddr_x *addr, socklen_t addrlen, void *buf, size_t buflen, off_t offset)
{
	size_t to_copy;
	xcache_cmd cmd;

	cmd.set_cmd(xcache_cmd::XCACHE_READ);
	cmd.set_context_id(h->contextID);
	cmd.set_dag(addr, addrlen);
	cmd.set_readoffset(offset);
	cmd.set_readlen(buflen);

	if(send_command(h->xcacheSock, &cmd) < 0) {
				fprintf(stderr, "Error in sending command to xcache\n");
		/* Error in Sending chunk */
		return -1;
	}

	if(get_response_blocking(h->xcacheSock, &cmd) < 0) {
		fprintf(stderr, "Did not get a valid response from xcache\n");
		return -1;
	}

	to_copy = MIN(cmd.data().length(), buflen);
	memcpy(buf, cmd.data().c_str(), to_copy);
	fprintf(stderr, "Copying %lu bytes of %lu to buffer\n", to_copy, buflen);

	return to_copy;
}
