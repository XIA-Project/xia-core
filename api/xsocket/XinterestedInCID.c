#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include <errno.h>
/*!
 * @brief Send a interest request for specified CID
 *
 * Send an ICID - Interested in CID packet to the CID source. ICID is a
 * separate principal type that is handled by Xcache. Any in-path router
 * can choose to serve the content if it is available. This API is distinct
 * from XfetchChunk because it simply expresses interest in a CID that
 * will be pushed asynchronously, if available.
 *
 * @param sockfd the socket to which chunk will be pushed when available
 * @param addr of the chunk that is being requested
 *
 * @returns 0 if the interest request was successfully sent
 * @returns -1 on failure
 */
int XinterestedInCID(int sockfd, sockaddr_x *addr)
{
	int sock;
	int rc = -1;
	int state = 0;
	struct addrinfo *ai;
	char src_SID[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	bzero(src_SID, sizeof(src_SID));

	xia::XSocketMsg xsm;
	xia::X_CIDInterest_Msg *interest;

	// Create a message to be sent to Click
	xsm.set_type(xia::XCIDINTEREST);
	unsigned seq = seqNo(sockfd);
	xsm.set_sequence(seq);

	interest = xsm.mutable_x_cidinterest();

	// Make sure we have a good socket that can receive chunk contents
	if(addr->sx_family != AF_XIA) {
		errno = EAFNOSUPPORT;
		return -1;
	}

	// Make sure the requested CID address is valid
	Graph g((sockaddr_x *)addr);
	if(g.num_nodes() <= 0) {
		errno = EADDRNOTAVAIL;
		return -1;
	}

	// Caller must be listening on the given socket
	if(!isSIDAssigned(sockfd)) {
		LOG("XinterestedInCID: Error: provided socket not bound\n");
		return -1;
	}

	// API specific values
	interest->set_cid_addr(addr, sizeof(sockaddr_x));

	if ((rc = click_send(sockfd, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		return -1;
	} else if((rc = click_reply(sockfd, seq, &xsm)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		return -1;
	}

XinterestedInCID_done:
	return 0;
}
