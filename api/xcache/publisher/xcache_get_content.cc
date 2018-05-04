// Project headers
#include "headers/ncid_header.h"

// C++ headers
#include <iostream>
#include <atomic>
#include <future>

// C Headers
#include <stdlib.h>

#define XCACHE_READ_TIMEOUT_SEC 1
#define XCACHE_READ_TIMEOUT_USEC 0
#define XCACHE_MAX_IO_BUF_SIZE 65535

// Return number of bytes read
// -1 on error or stopping
int interruptible_read(int sock, void *buf, size_t len,
		std::atomic<bool>& stop)
{
	fd_set rfds;
	struct timeval timeout;
	int rc;

	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);

	timeout.tv_sec = XCACHE_READ_TIMEOUT_SEC;
	timeout.tv_usec = XCACHE_READ_TIMEOUT_USEC;

	while(!stop) {
		rc = Xselect(sock+1, &rfds, NULL, NULL, &timeout);
		if(rc == 0) {
			// timed out, try waiting again after checking for stop
			continue;
		} else if(rc < 0 || rc > 1) {
			std::cout << "ERROR Select on read returned " << rc << std::endl;
			return -1;
		}
		// We get here only if rc is 1
		assert(rc == 1);
		// Break out and perform the requested read
		break;
	}

	// Return an error if this read is being interrupted
	if(stop) {
		return -1;
	}

	// Make one read attempt and return the number of bytes read
	rc = Xrecv(sock, buf, len, 0);
	// Note: Xrecv may return -1 on failure
	return rc;
}

// Returns 0 if succeeded in reading exactly len bytes
// -1 if an error/timeout/stop occurs
int reliable_read(int sock, char *buf, int len, std::atomic<bool>& stop)
{
	int remaining = len;
	size_t offset = 0;
	int count;

	while(remaining) {
		count = interruptible_read(sock, buf+offset, remaining, stop);
		offset += count;
		remaining -= count;
		if(count == 0) {
			break;
		}
	}
	if(remaining != 0) {
		std::cout << "ERROR: Unable to read bytes: " << len << std::endl;
		return -1;
	}
	return 0;
}

int do_chunk_transfer(int sock, std::string &ubuf,
		std::unique_ptr<ContentHeader> &chdr,
		std::atomic<bool>& stop)
{
	int state = 0;
	int retval = -1;
	char *buf;
	std::string headerstr;
	ContentHeaderBuf chdr_buf;

	// Read the content header length
	uint32_t chdr_len = 0;
	if(reliable_read(sock, (char *)&chdr_len, sizeof(chdr_len), stop)) {
		std::cout << "ERROR fetching content header size" << std::endl;
		goto do_chunk_transfer_done;
	}
	chdr_len = ntohl(chdr_len);
	if(chdr_len == 0) {
		std::cout << "ERROR Empty header for requested content" << std::endl;
		goto do_chunk_transfer_done;
	}

	// Read the content header
	buf = (char *)calloc(1, chdr_len);
	if(buf == NULL) {
		std::cout << "ERROR allocating memory for content header" << std::endl;
		goto do_chunk_transfer_done;
	}
	state = 1;	// buf allocated

	std::cout << "Fetching content header of size " << chdr_len << std::endl;
	if(reliable_read(sock, buf, chdr_len, stop)) {
		std::cout << "ERROR fetching content header" << std::endl;
		goto do_chunk_transfer_done;
	}

	// Process the content header into an object
	headerstr.assign(buf, chdr_len);
	if(chdr_buf.ParseFromString(headerstr) == false) {
		std::cout << "ERROR bad content header data" << std::endl;
		goto do_chunk_transfer_done;
	}
	if(chdr_buf.has_cid_header()) {
		chdr.reset(new CIDHeader(headerstr));
	} else if(chdr_buf.has_ncid_header()) {
		chdr.reset(new NCIDHeader(headerstr));
	} else {
		assert(0);
	}

	// Allocate memory for the content
	free(buf);
	buf = (char *)calloc(1, chdr->content_len());
	if(buf == NULL) {
		std::cout << "ERROR allocating memory to hold content" << std::endl;
		goto do_chunk_transfer_done;
	}

	if(reliable_read(sock, buf, chdr->content_len(), stop)) {
		std::cout << "ERROR reading chunk contents" << std::endl;
		goto do_chunk_transfer_done;
	}
	ubuf.assign(buf, chdr->content_len());

	// Return content header and content
	retval = 0;

do_chunk_transfer_done:
	switch(state) {
		case 1: free(buf);
	};

	return retval;
}

/*!
 * @brief Read content header and content from a connected socket
 *
 * Given a socket that has already been connected to a remote source
 * of a chunk, read the content header and the chunk contents.
 * If the contents are valid, return success.
 *
 * The stop argument allows this call to be interrupted by the caller.
 * Since this thread will block, the call will need a separate thread
 * to trigger the stop.
 *
 * Since the caller has no way of knowing the header or chunk size in
 * advance, this function allocates the memory.
 * NOTE: Caller must free "buf" and "chdr" if this function returns success
 *
 * @param sock a socket connected to a process sending a chunk
 * @param buf will hold chunk contents. Caller must free on success.
 * @param len holds the size of chunk contents.
 * @param chdr a pointer to a ContentHeader object. Caller must delete.
 *
 * @returns 0 on success
 * @returns -1 on failure
 */
int xcache_get_content(int sock,
		std::string &buf,
		std::unique_ptr<ContentHeader> &chdr,
		std::atomic<bool>& stop)
{
	/*
	// Create a job to fetch the chunk asynchronously
	std::future<int> rc(std::async(std::launch::async,
				do_chunk_transfer, sock, buf, len, chdr, stop));

	// Wait for the async job to finish
	return rc.get();
	*/
	return do_chunk_transfer(sock, buf, chdr, stop);
}
