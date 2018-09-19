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

	// Book-keeping entries
	unsigned id = getID(sockfd);
	interest->set_id(id);

	// Make sure we have been a good socket that can receive chunk contents
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

	// Assign a DAG for sockfd, if one doesn't exist already
	if(!isSIDAssigned(sockfd)) {
		LOG("XinterestedInCID: creating new SID\n");
		if(XmakeNewSID(src_SID, sizeof(src_SID))) {
			LOG("Unable to create a new SID");
			return -1;
		}
		LOGF("XinterestedInCID: new SID:%s:", src_SID);

		// Build a default DAG with this SID as our source address
		if(Xgetaddrinfo(NULL, src_SID, NULL, &ai)) {
			LOGF("Unable to make default DAG for %s", src_SID);
			return -1;
		}
		sockaddr_x *sa = (sockaddr_x *)ai->ai_addr;
		Graph src_dag(sa);
		LOGF("XinterestedInCID: our addr:%s:\n", src_dag.dag_string().c_str());

		// Include the source DAG in the message to Click
		interest->set_src_addr(sa, sizeof(sockaddr_x));

		Xfreeaddrinfo(ai);
		setSIDAssigned(sockfd);
		setTempSID(sockfd, src_SID);
	}

	// API specific values
	interest->set_cid_addr(addr, sizeof(sockaddr_x));

	sock = MakeApiSocket(SOCK_DGRAM);
	// TODO check if MakeApiSocket succeeded here and increment state
	state = 1; // sock allocated

	if ((rc = click_send(sock, &xsm)) < 0) {
		LOGF("Error talking to Click: %s", strerror(errno));
		goto XinterestedInCID_done;
	} else if((rc = click_reply(sock, seq, &xsm)) < 0) {
		LOGF("Error getting status from Click: %s", strerror(errno));
		goto XinterestedInCID_done;
	}

XinterestedInCID_done:
	switch(state) {
		case 1:
			freeSocketState(sock);
			(_f_close)(sock);
			/* fall through */
	}

	return rc;
}
