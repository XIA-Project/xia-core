#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <syslog.h>
#include "Xsocket.h"
#include <iostream>
#include "controller.h"
#include "cache.h"
#include "cid.h"
#include "ncid_header.h"
#include <clicknet/xia.h>
#include <clicknet/xtcp.h>

#define CACHE_SOCK_NAME "/tmp/xcache-click.sock"
#define CACHEFILTER_SOCK_NAME "/tmp/cachefilter-click.sock"

void xcache_cache::spawn_thread(struct cache_args *args)
{
	pthread_t cache;

	pthread_create(&cache, NULL, run, (void *)args);
}

/*!
 * @brief create control socket to talk to CacheFilter in Click
 *
 * Simply create a socket that will be used to send control packets
 * to CacheFilter in Click. The CacheFilter binds to this socket
 * as a server and receives any packets we send to it.
 *
 * @returns socket descriptor on success, -1 on failure
 */
int xcache_cache::create_cachefilter_socket()
{
	int s;
	if ((s = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
		syslog(LOG_ERR, "Can't create socket: %s", strerror(errno));
		return -1;
	}
	return s;
}

/*!
 * @brief create bound socket to receive in-flight packets
 *
 * Create a socket bound to CACHE_SOCK_NAME. CacheFilter in Click
 * will forward in-flight packets to this socket.
 *
 * @returns socket descriptor on success, -1 on failure
 */
int xcache_cache::create_click_socket()
{
	int s;

	if ((s = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
		syslog(LOG_ERR, "can't create click socket: %s", strerror(errno));
		return -1;
	}

	sockaddr_un sa;
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strcpy(sa.sun_path, CACHE_SOCK_NAME);

	if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
		syslog(LOG_ERR, "unable to bind: %s", strerror(errno));
		return -1;
	}

	syslog(LOG_INFO, "Created cache socket on %s\n", CACHE_SOCK_NAME);

	return s;
}

void xcache_cache::unparse_xid(struct click_xia_xid_node *node, std::string &xid)
{
	char hex[41];
	unsigned char *id = node->xid.id;
	char *h = hex;

	while (id - node->xid.id < CLICK_XIA_XID_ID_LEN) {
		sprintf(h, "%02x", (unsigned char)*id);
		id++;
		h += 2;
	}

	hex[CLICK_XIA_XID_ID_LEN * 2] = 0;
	xid = std::string(hex);
}

/*!
 * @brief validate a packet and retrieve header information from it
 *
 * @param pkt the buffer containing the packet
 * @param len length of the packet
 * @returns chunk identifier - could be CID or NCID
 * @returns sid service identifier of the requesting client process
 * @returns xtcp the TCP header
 * @returns PACKET_OK on success, error code on failure
 */
int xcache_cache::validate_pkt(char *pkt, size_t len,
		std::string &cid, std::string &sid, struct xtcp **xtcp)
{
	struct click_xia *xiah = (struct click_xia *)pkt;
	struct xtcp *x;
	unsigned total_nodes;
	size_t xip_size;

	*xtcp = NULL;

	// Check if the packet is too small
	if ((len < sizeof(struct click_xia)) || (htons(xiah->plen) > len) ) {
		// packet is too small, this had better not happen!
		return PACKET_INVALID;
	}

	// Retrieve the pointer to TCP header in packet
	total_nodes = xiah->dnode + xiah->snode;
	xip_size = sizeof(struct click_xia) + (total_nodes * sizeof(struct click_xia_xid_node));
	x = (struct xtcp *)(pkt + xip_size);

	// Verify TCP flags
	uint16_t flags = ntohs(x->th_flags);
	// we only see the flow from server to client,
	// so we'll never see a plain SYN here
	// FIXME: we should probably be smart enough to deal with other flags
	// like RST here eventually
	if (flags == XTH_ACK) {
		ushort hlen = (ushort)(x->th_off) << 2;

		if (hlen == ntohs(xiah->plen)) {
			// it's empty and not a SYN-ACK or FIN
			// so we can ignore it
			return PACKET_NO_DATA;
		}
	} else if (flags == (XTH_SYN|XTH_ACK)) {
		// packet is OK, keep going

	} else if (!(flags & XTH_FIN)) {
		// if it's not a FIN*, we don't want it
		return PACKET_NO_DATA;
	}

	Graph dst_dag;
	dst_dag.from_wire_format(xiah->dnode, &xiah->node[0]);

	Graph src_dag;
	src_dag.from_wire_format(xiah->snode, &xiah->node[xiah->dnode]);

	// Returning - Client service that requested the chunk
	sid = dst_dag.intent_SID_str();

	// Returning - Chunk being transmitted - CID or NCID
	Node src_intent_xid = src_dag.get_final_intent();
	if (src_intent_xid.type() == CLICK_XIA_XID_TYPE_CID
			|| src_intent_xid.type() == CLICK_XIA_XID_TYPE_NCID) {
		cid = src_intent_xid.to_string();
	} else {
		syslog(LOG_ERR, "Dest DAG Intent not a CID/NCID");
		return PACKET_INVALID;
	}

	// Drop packets that don't have a stream header
	if (xiah->nxt != CLICK_XIA_NXT_XSTREAM) {
		syslog(LOG_INFO, "%s: not a stream packet, ignoring...", cid.c_str());
		return PACKET_INVALID;
	}

	// syslog(LOG_INFO, "XIA version = %d\n", xiah->ver);
	// syslog(LOG_INFO, "XIA plen = %d len = %lu\n", htons(xiah->plen), len);
	// syslog(LOG_INFO, "XIA nxt = %d\n", xiah->nxt);
	// syslog(LOG_INFO, "tcp->th_ack = %u\n", ntohl(x->th_ack));
	// syslog(LOG_INFO, "tcp->th_seq = %u\n", ntohl(x->th_seq));
	// syslog(LOG_INFO, "tcp->th_flags = %08x\n", ntohs(x->th_flags));
	// syslog(LOG_INFO, "tcp->th_off = %d\n", x->th_off);
	// syslog(LOG_INFO, "CID = %s\n", cid.c_str());
	// syslog(LOG_INFO, "SID = %s\n", sid.c_str());


	// Returning a pointer to the TCP header
	*xtcp = x;

	return PACKET_OK;
}

/*!
 * @brief New metadata to track an ongoing download for a CID/NCID
 *
 * @param tcp the TCP header for the packet being processed
 * @param cid Content identifier - could be CID or NCID
 * @param sid Client process service identifier
 *
 * @returns download object that was just created, on success
 * @returns NULL on failure
 */
cache_download* xcache_cache::start_new_download(struct xtcp *tcp,
		std::string cid, std::string sid)
{
	syslog(LOG_INFO, "Cache: starting new download for %s", cid.c_str());
	// if it's not a syn-ack we don't know how much content has already gone by
	if (!(ntohs(tcp->th_flags) & XTH_SYN)) {
		syslog(LOG_INFO, "skipping %s: partial stream received", cid.c_str());
		return NULL;
	}

	// FIXME: this should be wrapped in a mutex
	cache_download *download = NULL;

	// CID not filled in at this time because it could be an NCID
	xcache_meta *meta = new xcache_meta();
	if(meta == NULL) {
		syslog(LOG_INFO, "Unable to allocate memory for metadata");
		return NULL;
	}

	meta->set_state(CACHING);
	meta->set_seq(ntohl(tcp->th_seq));
	meta->set_dest_sid(sid);

	download = new cache_download();
	download->set_meta(meta);
	ongoing_downloads[cid] = download;

	return download;
}

/*!
 * @brief Blacklist the flow that this packet belongs to
 *
 * Ask CacheFilter in Click to block packets related to this flow.
 */
void xcache_cache::blacklist(int cfsock, char *pkt, size_t len)
{
	// Retrieve source and destination DAGs from the packet
	struct click_xia *xiah = (struct click_xia *)pkt;
	Graph dst_dag, src_dag;
	if(len < sizeof(click_xia) +
			((xiah->dnode + xiah->snode) * sizeof(click_xia_xid_node))) {
		syslog(LOG_ERR, "Cache::blacklist Packet too short");
		return;
	}
	dst_dag.from_wire_format(xiah->dnode, &xiah->node[0]);
	src_dag.from_wire_format(xiah->snode, &xiah->node[xiah->dnode]);

	// Retrieve intent nodes from the source and destination DAGs
	Node dst_intent = dst_dag.get_final_intent();
	Node src_intent = src_dag.get_final_intent();

	// SessionID = <src_intent> + <dst_intent> = CID + SIDclient
	std::string session_id = src_intent.to_string() + dst_intent.to_string();
	syslog(LOG_INFO, "Cache: blacklisting:%s:", session_id.c_str());

	// Build address of CacheFilter to send a packet to it
	sockaddr_un sa;
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strcpy(sa.sun_path, CACHEFILTER_SOCK_NAME);

	ssize_t sent = sendto(cfsock, session_id.c_str(), session_id.size(), 0,
			(struct sockaddr *) &sa, sizeof(sa));
	if(sent != (ssize_t) session_id.size()) {
		syslog(LOG_ERR, "Cache: Sent %zu to Click instead of %zu", sent,
					session_id.size());
	}
}

/*!
 * @brief Process a packet that we are eavesdropping on
 *
 * @param cfsock socket to talk to CacheFilter in Click
 * @param ctrl a reference to the xcache controller
 * @param pkt the buffer containing the packet being observed
 * @param len length of the packet buffer
 */
void xcache_cache::process_pkt(int cfsock,
		xcache_controller *ctrl, char *pkt, size_t len)
{
	int rc;
	struct xtcp *tcp;
	std::string chunk_id = "";
	std::string sid = "";
	uint32_t chdr_len = 0;
	size_t payload_len;
	size_t offset;
	size_t data_offset;
	xcache_meta *meta;
	cache_download *download;
	std::map<std::string, cache_download *>::iterator downloads_it;

	syslog(LOG_DEBUG, "CACHE RECVD PKT\n");

	// Retrieve chunk_id, sid and TCP header from a valid packet
	rc = validate_pkt(pkt, len, chunk_id, sid, &tcp);
	switch (rc) {
	case PACKET_OK:
		// keep going, it's either a SYN-ACK or contains data
		break;

	case PACKET_NO_DATA:
		// it's a pure ACK, we can ignore it
		// syslog(LOG_INFO, "data packet is empty!");
		return;

	case PACKET_INVALID:
	default:
		// the packet is not not a stream pkt or is malformed
		// we shouldn't see this in real usage
		syslog(LOG_ERR, "packet doesn't contain valid cid data");
		return;
	}

	meta = ctrl->acquire_meta(chunk_id);
	if(meta != NULL) {
		ctrl->release_meta(meta);
		syslog(LOG_INFO, "%s In-flight chunk known to controller. Blacklist",
				chunk_id.c_str());
		blacklist(cfsock, pkt, len);
		return;
	}
	meta = NULL; // we reuse meta to point to in-flight chunk meta later

	char *payload = (char *)tcp + ((ushort)(tcp->th_off) << 2);
	payload_len = len + (size_t)(pkt - payload);
	syslog(LOG_INFO, "%s Payload length = %lu\n",
			chunk_id.c_str(), payload_len);

	downloads_it = ongoing_downloads.find(chunk_id);
	if(downloads_it == ongoing_downloads.end()) {
		download = start_new_download(tcp, chunk_id, sid);
		if(download == NULL) {
			syslog(LOG_ERR, "failed creating download for %s",
					chunk_id.c_str());
		}
		// This was a syn-ack, so there is not data to handle yet
		return;
	} else {
		download = downloads_it->second;
	}
	// TODO: Verify that CACHING is the only possible state
	// If yes, then remove this switch
	switch (download->meta()->state()) {
		case CACHING:
			// drop into the code below the switch
			break;

		case AVAILABLE:
		case READY_TO_SAVE:
		case FETCHING:
			//syslog(LOG_INFO, "This CID is already in the cache: %s", cid.c_str());
			return;

		case EVICTING:
			syslog(LOG_INFO, "The CID is in process of being evicted: %s", chunk_id.c_str());
			return;

		default:
			syslog(LOG_ERR, "Some Unknown STATE FIX IT CID=%s", chunk_id.c_str());
			return;
	}

	// Ignore this stream if another stream is already getting the chunk
	if (download->meta()->dest_sid() != sid) {
		// Another stream is already getting this chunk.
		//  so we can ignore this stream's data
		//  otherwise we'll have memory overrun issues due to different sequence #s
		// syslog(LOG_INFO, "don't cross the streams!");
		blacklist(cfsock, pkt, len);
		return;
	}

	// get the initial sequence number for this stream
	uint32_t initial_seq = download->meta()->seq();
	syslog(LOG_INFO, "Initial seq no:%u", initial_seq);

	// We got "payload" of length "payload_len"
	// This is at offset ntohl(tcp->th_seq) in total chunk
	// Adjusted sequence numbers from 1 to sizeof(struct cid_header)
	//  constitute the CID header. Everything else is payload
	if (payload_len == 0) {
		syslog(LOG_INFO, "Skipping data");
		// control packet
		goto skip_data;
	}

	syslog(LOG_INFO, "Payload len:%zu", payload_len);
	// FIXME: this doesn't deal with the case where the sequence number wraps.
	// factor out the random start value of the seq #
	offset = ntohl(tcp->th_seq) - initial_seq;
	syslog(LOG_INFO, "Stream packet offset:%zu", offset);
	//syslog(LOG_INFO, "initial=%u, seq = %u offset = %lu\n", initial_seq, ntohl(tcp->th_seq), offset);

	// ASSUMPTION: First packet received is the one with header length
	//
	// We found the first packet, get content header length from it
	if(offset == 0) {

		// Get the content header length
		assert(payload_len >= sizeof(chdr_len));
		memcpy(&chdr_len, payload, sizeof(chdr_len));
		chdr_len = ntohl(chdr_len); // convert from network to host order
		download->set_chdr_len(chdr_len);
		syslog(LOG_INFO, "Content header len:%u", download->chdr_len());

		// Allocate memory to hold the content header
		download->set_chdr_data((char *)calloc(download->chdr_len(), 1));
		assert(download->chdr_data() != NULL);
		syslog(LOG_INFO, "ContentHeader stored at:%lu",
				(long unsigned int)download->chdr_data());
	}

	// Unless the content header is received, we can't receive header or data
	if(download->chdr_len() == 0) {
		syslog(LOG_INFO, "Data packet received before header, dropping");
		return;
	}

	// The offset in stream where content starts, after the header
	data_offset = sizeof(chdr_len) + download->chdr_len();
	syslog(LOG_INFO, "Data offset:%zu", data_offset);

	// Offset is somewhere in the content header
	if(offset < data_offset && offset >= sizeof(chdr_len)) {
		// Only save off until end of content header
		if((offset + payload_len) >= data_offset) {
			len = data_offset - offset;
		} else {
			len = payload_len;
		}
		memcpy(download->chdr_data() + offset - sizeof(chdr_len),
				payload, len);
		syslog(LOG_INFO, "Copying chdr contents to addr:%lu size:%zu",
				(long unsigned int)(download->chdr_data()
					+ offset - sizeof(chdr_len)), len);
		payload += len;
		offset += len;
		payload_len -= len;
	}

	// there's data, append it to the download buffer
	if (offset >= data_offset) {

		// Allocate memory to hold data
		if (download->data_buf() == NULL) {

			// Deserialize the ContentHeader received earlier
			if(download->chdr() == NULL) {
				std::string headerstr(download->chdr_data(),
						download->chdr_len());

				Node content_id(chunk_id);
				switch(content_id.type()) {
					case CLICK_XIA_XID_TYPE_NCID:
						syslog(LOG_INFO, "Created NCIDHeader");
						download->set_chdr(new NCIDHeader(headerstr));
						break;
					case CLICK_XIA_XID_TYPE_CID:
						syslog(LOG_INFO, "Created CIDHeader");
						download->set_chdr(new CIDHeader(headerstr));
						break;
					default:
						assert(0);
				}
			}

			// Now allocate space to hold data, based on content len in header
			download->set_data_buf((char *)calloc(download->chdr()->content_len(), 1));
			syslog(LOG_INFO, "Chunk data addr: %lu size %zu",
					(long unsigned int)download->data_buf(),
					download->chdr()->content_len());
		}

		// Copy data based on stream offset. Allows out-of-order packets
		memcpy(download->data_buf() + offset - data_offset,
				payload, payload_len);
		syslog(LOG_INFO, "Copying %zu bytes of chunk data to addr %lu",
				payload_len,
				(long unsigned int)(download->data_buf()+offset-data_offset));
	}

skip_data:
	if ((ntohs(tcp->th_flags) & XTH_FIN)) {
		// FIN Received, cache the chunk

		meta = download->meta();
		syslog(LOG_INFO, "meta in download points to %lu",
				(long unsigned int) meta);
		assert(download->chdr() != NULL);
		meta->set_content_header(download->chdr());
		// Convert downloaded data into a string
		std::string chunk_datastr(download->data_buf(),
				download->chdr()->content_len());
		if(meta->valid_data(chunk_datastr)) {
			syslog(LOG_INFO, "chunk is valid: %s", chunk_id.c_str());

			meta->set_ttl(download->chdr()->ttl());
			meta->set_created();
			meta->set_length(download->chdr()->content_len());
			meta->set_state(READY_TO_SAVE);
			syslog(LOG_INFO, "saving metadata for CID:%s",
					meta->store_id().c_str());
			ctrl->add_meta(meta);
			assert(ctrl->acquire_meta(chunk_id) != NULL);
			// printf("cache:cache length: %lu\n", meta->get_length());

			xcache_req *req = new xcache_req();
			req->type = xcache_cmd::XCACHE_CACHE;
			req->cid = strdup(chunk_id.c_str());
			req->data = download->data_buf();
			req->datalen = download->chdr()->content_len();
			ctrl->enqueue_request_safe(req);

			/* Perform cleanup */
			downloads_it = ongoing_downloads.find(chunk_id);
			if(downloads_it != ongoing_downloads.end()) {
				ongoing_downloads.erase(downloads_it);
			}
			delete download;
		} else {
			// Drop everything related to this chunk being cached.
			//
			// Not waiting for the small possibility that some data was
			// still in flight but out-of-order after the FIN.
			//
			// If we want to support that scenario, then support it fully
			// instead of leaving this partial chunk hanging.
			syslog(LOG_ERR, "Invalid chunk, discarding: %s", chunk_id.c_str());
			downloads_it = ongoing_downloads.find(chunk_id);
			if(downloads_it != ongoing_downloads.end()) {
				ongoing_downloads.erase(downloads_it);
			}
			download->destroy();
			delete download;
		}
	}
}

void *xcache_cache::run(void *arg)
{
	struct cache_args *args = (struct cache_args *)arg;
	int ret;
	int csock, cfsock;
	char buffer[XIA_MAXBUF];

	(void)args;

	if ((csock = create_click_socket()) < 0) {
		syslog(LOG_ALERT, "Failed to create a xcache:click socket\n");
		pthread_exit(NULL);
	}
	if ((cfsock = create_cachefilter_socket()) < 0) {
		syslog(LOG_ERR, "Failed creating sock to talk to CacheFilter\n");
		pthread_exit(NULL);
	}

	do {
		syslog(LOG_DEBUG, "Cache listening for data from click\n");
		ret = recvfrom(csock, buffer, XIA_MAXBUF, 0, NULL, NULL);
		if(ret < 0) {
			syslog(LOG_ERR, "Error while reading from socket: %s\n", strerror(errno));
			pthread_exit(NULL);
		} else if (ret == 0) {
			// we should probably error out here too
			syslog(LOG_ERR, "no data: %s\n", strerror(errno));
			continue;
		}

		syslog(LOG_DEBUG, "Cache received a message of size = %d\n", ret);
		args->cache->process_pkt(cfsock, args->ctrl, buffer, ret);

	} while(1);
}
