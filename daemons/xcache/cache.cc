#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#include "Xsocket.h"
#include <iostream>
#include "controller.h"
#include "cache.h"
#include "cid.h"
#include <clicknet/xia.h>
#include <clicknet/xtcp.h>

void xcache_cache::spawn_thread(struct cache_args *args)
{
	pthread_t cache;

	pthread_create(&cache, NULL, run, (void *)args);
}

int xcache_cache::create_click_socket(int port)
{
	struct sockaddr_in si_me;
	int s, optval = 1;

	if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;

	memset((char *)&si_me, 0, sizeof(si_me));
	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(port);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	if (bind(s, (struct sockaddr *)&si_me, sizeof(si_me)) == -1)
		return -1;

	syslog(LOG_INFO, "Created cache socket on %d\n", port);

	return s;
}

void xcache_cache::process_pkt(xcache_controller *ctrl, char *pkt, size_t len)
{
	int total_nodes, i;
	struct click_xia *xiah = (struct click_xia *)pkt;
	struct xtcp *tcp;
	std::string cid = "";
	int payload_len;
	xcache_meta *meta;
	struct cache_download *download;
	std::map<std::string, struct cache_download *>::iterator iter;
	size_t offset;

	syslog(LOG_INFO, "CACHE RECVD PKT\n");
	syslog(LOG_INFO, "XIA version = %d\n", xiah->ver);
	syslog(LOG_INFO, "XIA plen = %d\n", htons(xiah->plen));
	syslog(LOG_INFO, "XIA nxt = %d\n", xiah->nxt);

	total_nodes = xiah->dnode + xiah->snode;

	for (i = 0; i < total_nodes; i++) {
		uint8_t id[20];
		char hex_string[41];

		switch (htonl(xiah->node[i].xid.type)) {
			case CLICK_XIA_XID_TYPE_CID:
				memcpy(id, xiah->node[i].xid.id, 20);
				for(int j = 0; j < 20; j++)
					sprintf(&hex_string[2*j], "%02x",
						(unsigned int)id[j]);
				cid = std::string(hex_string);
				break;
			default:
				break;
		};
	}

	tcp = (struct xtcp *)
		((char *)xiah + sizeof(struct click_xia) +
		 (total_nodes) * sizeof(struct click_xia_xid_node));

	syslog(LOG_INFO, "tcp->th_ack = %u\n", ntohl(tcp->th_ack));
	syslog(LOG_INFO, "tcp->th_seq = %u\n", ntohl(tcp->th_seq));
	syslog(LOG_INFO, "tcp->th_flags = %08x\n", ntohs(tcp->th_flags));
	syslog(LOG_INFO, "tcp->th_off = %d\n", tcp->th_off);

	syslog(LOG_INFO, "CID=%s\n", cid.c_str());

	char *payload = (char *)tcp + tcp->th_off * 4;
	payload_len = (unsigned long)pkt + len - (unsigned long)payload;

	syslog(LOG_INFO, "Payload length = %d\n", payload_len);

	meta = ctrl->acquire_meta(cid);
	if (!meta) {
		syslog(LOG_INFO, "ACCEPTING: New Meta\n");
		meta = new xcache_meta(cid);
		meta->set_OVERHEARING();
		ctrl->add_meta(meta);
		ctrl->acquire_meta(cid);
	} else if (meta->is_DENY_PENDING()) {
		syslog(LOG_INFO, "DENYING: Already Denied Meta\n");
		ctrl->release_meta(meta);
		return;
	} else if (meta->is_AVAILABLE()) {
		syslog(LOG_INFO, "DENYING: I Have this already\n");
		ctrl->release_meta(meta);
		// DENY CID
		return;
	} else if (meta->is_FETCHING()) {
		syslog(LOG_INFO, "DENYING: I am fetching it\n");
		ctrl->release_meta(meta);
		// DENY CID
		return;
	} else if (!meta->is_OVERHEARING()) {
		syslog(LOG_INFO, "Some Unknown STATE FIX IT\n");
	}
	ctrl->release_meta(meta);

	iter = ongoing_downloads.find(cid);
	if (iter == ongoing_downloads.end()) {
		syslog(LOG_INFO, "New Download\n");
		/* This is a new chunk */
		download = (struct cache_download *)
			malloc(sizeof(struct cache_download));
		memset(download, 0, sizeof(struct cid_header));
		ongoing_downloads[cid] = download;
	} else {
		syslog(LOG_INFO, "Download Found\n");
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

	/*
	 * FIXME: Why offset 2?
	 */
	offset = ntohl(tcp->th_seq) - 2;
	if (offset < sizeof(struct cid_header)) {
		len = offset + payload_len > sizeof(struct cid_header) ?
			sizeof(struct cid_header) : offset + payload_len;
		memcpy((char *)&download->header + offset, payload, len);

		payload += len;
		offset += len;
		payload_len -= len;
	}

	if (offset >= sizeof(struct cid_header)) {
		if (download->data == NULL)
			download->data = (char *)
				malloc(ntohl(download->header.length));
		memcpy(download->data + offset - sizeof(struct cid_header),
		       payload, payload_len);
	}


	syslog(LOG_INFO, "Header: off = %d, len = %d, total_len = %d\n",
		       ntohl(download->header.offset),
		       ntohl(download->header.length),
		       ntohl(download->header.total_length));


skip_data:
	if (htons(tcp->th_flags) & XTH_FIN) {
		/* FIN Received */
		std::string *data = new std::string(download->data,
						    ntohl(download->header.length));

		/* FIXME: Verify CID */

		ctrl->__store(NULL, meta, data);
		/* Perform cleanup */
		delete data;
		ongoing_downloads.erase(iter);
		free(download);
	}
}

void *xcache_cache::run(void *arg)
{
	struct cache_args *args = (struct cache_args *)arg;
	int s, ret;
	char buffer[XIA_MAXBUF];
	struct sockaddr_in fromaddr;
	socklen_t len = sizeof(fromaddr);
	struct sockaddr_in toaddr;

	(void)args;

	if ((s = create_click_socket(args->cache_out_port)) < 0) {
		syslog(LOG_ALERT, "Failed to create a socket on %d\n", args->cache_out_port);
		pthread_exit(NULL);
	}

	do {
		syslog(LOG_NOTICE, "Cache listening for data on port %d\n", args->cache_out_port);
		ret = recvfrom(s, buffer, XIA_MAXBUF, 0, (struct sockaddr *)&fromaddr, &len);
		if(ret <= 0) {
			syslog(LOG_ERR, "Error while reading from socket: %s\n", strerror(errno));
			continue;
		}

		syslog(LOG_INFO, "Cache received a message of size = %d\n", ret);

		args->cache->process_pkt(args->ctrl, buffer, ret);

		toaddr = fromaddr;
		toaddr.sin_port = htons(args->cache_in_port);

		//FIXME: Send meaningful messages
		ret = sendto(s, "Something", strlen("Something") + 1, 0, (struct sockaddr *)&toaddr, sizeof(toaddr));
		if(ret <= 0) {
			syslog(LOG_ERR, "Error while sending to click: %s", strerror(errno));
			continue;
		}
	} while(1);
}
