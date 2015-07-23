#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "logger.h"
#include <iostream>
#include "controller.h"
#include "cache.h"

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

void *xcache_cache::run(void *arg)
{
	struct cache_args *args = (struct cache_args *)arg;
	int s, ret;
	char buffer[1600];
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
		ret = recvfrom(s, buffer, 1600, 0, (struct sockaddr *)&fromaddr, &len);
		if(ret <= 0) {
			LOG_CACHE_ERROR("Error while reading from socket\n");
			continue;
		}

		toaddr = fromaddr;
		toaddr.sin_port = htons(args->cache_in_port);
		LOG_CACHE_INFO("Cache received a message\n");
		LOG_CACHE_INFO("Cache Sent a message\n");

		ret = sendto(s, "Something", strlen("Something") + 1, 0, (struct sockaddr *)&toaddr, sizeof(toaddr));
		if(ret <= 0) {
			LOG_CACHE_ERROR("Error while sending to click\n");
			continue;
		}
	} while(1);
}
