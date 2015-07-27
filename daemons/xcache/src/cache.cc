#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "Xsocket.h"
#include "logger.h"
#include <iostream>
#include "controller.h"
#include "cache.h"
#include "cid.h"

DEFINE_LOG_MACROS(CACHE)

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

	LOG_CACHE_INFO("Created cache socket\n");

    return s;
}

void xcache_cache::process_pkt(xcache_controller *ctrl, char *pkt, size_t len)
{
	struct cid_header *header = (struct cid_header *)pkt;
	std::string cid(header->cid);
	std::map<std::string, std::string *>::iterator i;
	xcache_meta *meta;

	(void)len;

//	LOG_CACHE_INFO("      Data = %s\n", pkt + sizeof(struct cid_header));
	LOG_CACHE_INFO("      Off = %lu, Len = %lu\n", header->offset, header->length);
	LOG_CACHE_INFO("      CID = %s\n", header->cid);
	
	meta = ctrl->acquire_meta(cid);
	if(!meta) {
		LOG_CACHE_INFO("ACCEPTING: New Meta\n");
		meta = new xcache_meta(cid);
		meta->set_OVERHEARING();
		ctrl->add_meta(meta);
		ctrl->acquire_meta(cid);
	} if(meta->is_DENY_PENDING()) {
		LOG_CACHE_INFO("DENYING: Already Denied Meta\n");
		ctrl->release_meta(meta);
		return;
	} else if(meta->is_AVAILABLE()) {
		LOG_CACHE_INFO("DENYING: I Have this already\n");
		ctrl->release_meta(meta);
		// DENY CID
		return;
	} else if(meta->is_FETCHING()) {
		LOG_CACHE_INFO("DENYING: I am fetching it\n");
		ctrl->release_meta(meta);
		// DENY CID
		return;
	} else {
		LOG_CACHE_INFO("Some Unknown STATE FIX IT\n");
	}
	ctrl->release_meta(meta);

	/* We are interested in caching the packet */

	std::string *data;
	i = ongoing_downloads.find(cid);
	if(i == ongoing_downloads.end()) {
		/* This is a new chunk */
		data = new std::string("");
		ongoing_downloads[cid] = data;
	} else {
		data = i->second;
	}


	data->append(pkt + sizeof(struct cid_header), header->length);
	if(data->length() == header->total_length) {
		LOG_CACHE_INFO("Chunk Downloaded\n");
		ctrl->__store(NULL, meta, data);
	} else {
		LOG_CACHE_INFO("%d Bytes Remaining\n", header->total_length - data->length());
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

	if((s = create_click_socket(args->cache_out_port)) < 0) {
		LOG_CACHE_ERROR("Failed to create a socket on %d\n", args->cache_out_port);
		pthread_exit(NULL);
	}

	do {
		LOG_CACHE_INFO("Cache listening for data on port %d\n", args->cache_out_port);
		ret = recvfrom(s, buffer, XIA_MAXBUF, 0, (struct sockaddr *)&fromaddr, &len);
		if(ret <= 0) {
			LOG_CACHE_ERROR("Error while reading from socket\n");
			continue;
		}

		LOG_CACHE_INFO("Cache received a message of size = %d\n", ret);
		
		args->cache->process_pkt(args->ctrl, buffer, ret);

		toaddr = fromaddr;
		toaddr.sin_port = htons(args->cache_in_port);

		ret = sendto(s, "Something", strlen("Something") + 1, 0, (struct sockaddr *)&toaddr, sizeof(toaddr));
		if(ret <= 0) {
			LOG_CACHE_ERROR("Error while sending to click\n");
			continue;
		}
	} while(1);
}
