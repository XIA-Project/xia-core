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
#include <clicknet/xia.h>
#include <clicknet/xtcp.h>

#define CACHE_SOCK_NAME "/tmp/xcache-click.sock"

void xcache_cache::spawn_thread(struct cache_args *args)
{
	pthread_t cache;

	pthread_create(&cache, NULL, run, (void *)args);
}

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

void xcache_cache::process_pkt(xcache_controller *ctrl, char *pkt, size_t len)
{
	int total_nodes, i;
	struct click_xia *xiah = (struct click_xia *)pkt;
	struct xtcp *tcp;
	std::string cid = "";
	std::string sid = "";
	int payload_len;
	xcache_meta *meta;
	struct cache_download *download;
	std::map<std::string, struct cache_download *>::iterator iter;
	size_t offset;

	//syslog(LOG_DEBUG, "CACHE RECVD PKT\n");
	//syslog(LOG_DEBUG, "XIA version = %d\n", xiah->ver);
	//syslog(LOG_DEBUG, "XIA plen = %d\n", htons(xiah->plen));
	//syslog(LOG_DEBUG, "XIA nxt = %d\n", xiah->nxt);

	total_nodes = xiah->dnode + xiah->snode;

	// FIXME: replace magic numbers below
	for (i = 0; i < total_nodes; i++) {
		uint8_t id[20];
		char hex_string[41];

		unsigned type = htonl(xiah->node[i].xid.type);

		if (type == CLICK_XIA_XID_TYPE_CID || type == CLICK_XIA_XID_TYPE_SID) {
			hex_string[40] = 0;
			memcpy(id, xiah->node[i].xid.id, 20);
			for(int j = 0; j < 20; j++) {
				sprintf(&hex_string[2*j], "%02x", (unsigned int)id[j]);
			}
			if (type == CLICK_XIA_XID_TYPE_CID) {
				cid = std::string(hex_string);
			} else {
				sid = std::string(hex_string);
			}
		}
	}

	tcp = (struct xtcp *)
		((char *)xiah + sizeof(struct click_xia) +
		 (total_nodes) * sizeof(struct click_xia_xid_node));

	//syslog(LOG_DEBUG, "tcp->th_ack = %u\n", ntohl(tcp->th_ack));
	//syslog(LOG_INFO, "tcp->th_seq = %u\n", ntohl(tcp->th_seq));
	//syslog(LOG_DEBUG, "tcp->th_flags = %08x\n", ntohs(tcp->th_flags));
	//syslog(LOG_DEBUG, "tcp->th_off = %d\n", tcp->th_off);

	char *payload = (char *)tcp + tcp->th_off * 4;
	payload_len = (unsigned long)pkt + len - (unsigned long)payload;

	syslog(LOG_INFO, "%s Payload length = %d\n", cid.c_str(), payload_len);

	meta = ctrl->acquire_meta(cid);

	if (!meta) {
		// FIXME: race condition if 2 streams enter at the same time
		//   move meta cretion into acquire_meta
		syslog(LOG_INFO, "ACCEPTING: New Meta CID=%s", cid.c_str());
		meta = new xcache_meta(cid);
		meta->set_OVERHEARING();
		meta->set_seq(ntohl(tcp->th_seq));
		meta->set_dest_sid(sid);
		ctrl->add_meta(meta);
		ctrl->acquire_meta(cid);

	} else if (meta->is_DENY_PENDING()) {
		ctrl->release_meta(meta);
		syslog(LOG_INFO, "Already Denied Meta CID=%s", cid.c_str());
		return;

	} else if (meta->is_AVAILABLE()) {
		ctrl->release_meta(meta);
		syslog(LOG_INFO, "This CID is already in the cache: %s", cid.c_str());
		return;

	} else if (meta->is_FETCHING()) {
		ctrl->release_meta(meta);
		syslog(LOG_INFO, "Already fetching this CID: %s", cid.c_str());
		return;

	} else if (meta->is_OVERHEARING()) {
		if (meta->dest_sid() != sid) {
			// Another stream is already getting this chunk.
			//  so we can ignore this stream's data
			//  otherwise we'll have memory overrun issues due to different sequence #s
			syslog(LOG_INFO, "don't cross the streams!");
			ctrl->release_meta(meta);
			return;
		}

	} else {
		syslog(LOG_ERR, "Some Unknown STATE FIX IT CID=%s", cid.c_str());
		ctrl->release_meta(meta);
		return;
	}

	uint32_t initial_seq = meta->seq();
	ctrl->release_meta(meta);

	iter = ongoing_downloads.find(cid);
	if (iter == ongoing_downloads.end()) {
		// FIXME: this should happen above when we see a new SYN
		//syslog(LOG_INFO, "New Download\n");
		/* This is a new chunk */
		download = (struct cache_download *)malloc(sizeof(struct cache_download));
		memset(download, 0, sizeof(struct cache_download));
		ongoing_downloads[cid] = download;
	} else {
		//syslog(LOG_INFO, "Download Found\n");
		download = iter->second;
	}

	/*
	 * We got "payload" of length "payload_len"
	 * This is at offset ntohl(tcp->th_seq) in total chunk
	 * Sequence numbers from 1 to sizeof(struct cid_header) constitute the
	 * CID header. Everything else constitutes the payload
	 */

	if (payload_len == 0)
		goto skip_data;

	// FIXME: this doesn't deal with the case where the sequence number wraps.

	// factor out the random start value of the seq #
	offset = ntohl(tcp->th_seq) - initial_seq;

	//syslog(LOG_INFO, "initial=%u, seq = %u offset = %lu\n", initial_seq, ntohl(tcp->th_seq), offset);

	if (offset < sizeof(struct cid_header)) {
		len = (offset + payload_len) > sizeof(struct cid_header) ?
			sizeof(struct cid_header) : offset + payload_len;
		memcpy((char *)&download->header + offset, payload, len);

		payload += len;
		offset += len;
		payload_len -= len;
	}

	if (offset >= sizeof(struct cid_header)) {
		if (download->data == NULL) {
			download->data = (char *)calloc(ntohl(download->header.length), 1);
		}
		memcpy(download->data + offset - sizeof(struct cid_header), payload, payload_len);
	}

skip_data:
	if ((ntohs(tcp->th_flags) & XTH_FIN)) {
		// FIN Received, cache the chunk
		std::string *data = new std::string(download->data, ntohl(download->header.length));

		if (compute_cid(download->data, ntohl(download->header.length)) == cid) {
			syslog(LOG_INFO, "chunk is valid: %s", cid.c_str());

			ctrl->__store(NULL, meta, data);

			/* Perform cleanup */
			delete data;
			ongoing_downloads.erase(iter);
			free(download->data);
			free(download);
		} else {
			syslog(LOG_ERR, "Invalid chunk, discarding: %s", cid.c_str());
		}
	}
}

void *xcache_cache::run(void *arg)
{
	struct cache_args *args = (struct cache_args *)arg;
	int s, ret;
	char buffer[XIA_MAXBUF];
	struct sockaddr_in fromaddr;
	socklen_t len = sizeof(fromaddr);

	(void)args;

	if ((s = create_click_socket()) < 0) {
		syslog(LOG_ALERT, "Failed to create a socket on %d\n", args->cache_out_port);
		pthread_exit(NULL);
	}

	do {
		syslog(LOG_DEBUG, "Cache listening for data on port %d\n", args->cache_out_port);
		ret = recvfrom(s, buffer, XIA_MAXBUF, 0, (struct sockaddr *)&fromaddr, &len);
		if(ret < 0) {
			syslog(LOG_ERR, "Error while reading from socket: %s\n", strerror(errno));
			pthread_exit(NULL);
		} else if (ret == 0) {
			// we should probably error out here too
			syslog(LOG_ERR, "no data: %s\n", strerror(errno));
			continue;
		}

		syslog(LOG_DEBUG, "Cache received a message of size = %d\n", ret);
		args->cache->process_pkt(args->ctrl, buffer, ret);

	} while(1);
}
