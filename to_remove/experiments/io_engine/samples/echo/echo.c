#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <sys/wait.h>
#include <numa.h>

#include "../../include/ps.h"

#define MAX_CPUS 32

int num_devices;
struct ps_device devices[MAX_DEVICES];

int num_devices_attached;
int devices_attached[MAX_DEVICES];

struct ps_handle handles[MAX_CPUS];

int my_cpu;
int sink;

int get_num_cpus()
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}

int bind_cpu(int cpu)
{
        cpu_set_t *cmask;
	struct bitmask *bmask;
	size_t n;
	int ret;

	n = get_num_cpus();

        if (cpu < 0 || cpu >= (int)n) {
		errno = -EINVAL;
		return -1;
	}

	cmask = CPU_ALLOC(n);
	if (cmask == NULL)
		return -1;

        CPU_ZERO_S(n, cmask);
        CPU_SET_S(cpu, n, cmask);

        ret = sched_setaffinity(0, n, cmask);

	CPU_FREE(cmask);

	/* skip NUMA stuff for UMA systems */
	if (numa_max_node() == 0)
		return ret;

	bmask = numa_bitmask_alloc(16);
	assert(bmask);

	numa_bitmask_setbit(bmask, cpu % 2);
	numa_set_membind(bmask);
	numa_bitmask_free(bmask);

	return ret;
}

void print_usage(char *argv0)
{
	fprintf(stderr, "Usage: %s [-s] <interface to echo> <...>\n",
			argv0);
	fprintf(stderr, "  -s option makes this program work as a sink\n");

	exit(2);
}

void parse_opt(int argc, char **argv)
{
	int i, j;

	if (argc < 2)
		print_usage(argv[0]);

	if (strcmp(argv[1], "-s") == 0) {
		sink = 1;
		printf("just dropping incoming packets...\n");
	}

	for (i = 1 + sink; i < argc; i++) {
		int ifindex = -1;

		for (j = 0; j < num_devices; j++) {
			if (strcmp(argv[i], devices[j].name) != 0)
				continue;

			ifindex = devices[j].ifindex;
			break;
		}

		if (ifindex == -1) {
			fprintf(stderr, "Interface %s does not exist!\n", argv[i]);
			exit(4);
		}

		for (j = 0; j < num_devices_attached; j++) {
			if (devices_attached[j] == ifindex)
				goto already_attached;
		}

		devices_attached[num_devices_attached] = ifindex;
		num_devices_attached++;

already_attached:
		;
	}

	assert(num_devices_attached > 0);
}

void handle_signal(int signal)
{
	struct ps_handle *handle = &handles[my_cpu];

	uint64_t total_rx_packets = 0;
	uint64_t total_tx_packets = 0;

	int i;
	int ifindex;

	usleep(10000 * (my_cpu + 1));

	for (i = 0; i < num_devices_attached; i++) {
		ifindex = devices_attached[i];
		total_tx_packets += handle->tx_packets[ifindex];
		total_rx_packets += handle->rx_packets[ifindex];
	}

	printf("----------\n");
	printf("CPU %d: %ld packets received, %ld packets transmitted\n", 
			my_cpu, total_rx_packets, total_tx_packets);
	
	for (i = 0; i < num_devices_attached; i++) {
		char *dev = devices[devices_attached[i]].name;
		ifindex = devices_attached[i];

		if (handle->tx_packets[ifindex] == 0)
			continue;

		printf("  %s: ", dev);
		
		printf("RX %ld packets "
				"(%ld chunks, %.2f packets per chunk)  ", 
				handle->rx_packets[ifindex],
				handle->rx_chunks[ifindex],
				handle->rx_packets[ifindex] / 
				  (double)handle->rx_chunks[ifindex]);

		printf("TX %ld packets "
				"(%ld chunks, %.2f packets per chunk)\n", 
				handle->tx_packets[ifindex],
				handle->tx_chunks[ifindex],
				handle->tx_packets[ifindex] / 
				  (double)handle->tx_chunks[ifindex]);
	}

	exit(0);
}

void echo()
{
	struct ps_handle *handle = &handles[my_cpu];
	struct ps_chunk chunk;

	int i;
	int working = 0;

	assert(ps_init_handle(handle) == 0);

	for (i = 0; i < num_devices_attached; i++) {
		struct ps_queue queue;
		if (devices[devices_attached[i]].num_rx_queues <= my_cpu)
			continue;

		if (devices[devices_attached[i]].num_tx_queues <= my_cpu) {
			printf("WARNING: xge%d has not enough TX queues!\n",
					devices_attached[i]);
			continue;
		}

		working = 1;
		queue.ifindex = devices_attached[i];
		queue.qidx = my_cpu;

		printf("attaching RX queue xge%d:%d to CPU%d\n", queue.ifindex, queue.qidx, my_cpu);
		assert(ps_attach_rx_device(handle, &queue) == 0);
	}

	if (!working)
		goto done;

	assert(ps_alloc_chunk(handle, &chunk) == 0);

	chunk.recv_blocking = 1;

	for (;;) {
		int ret;
		
		chunk.cnt = 64;
		ret = ps_recv_chunk(handle, &chunk);

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			if (!chunk.recv_blocking && errno == EWOULDBLOCK)
				break;

			assert(0);
		}

		if (!sink) {
			chunk.cnt = ret;
			ret = ps_send_chunk(handle, &chunk);
			assert(ret >= 0);
		}
	}

done:
	ps_close_handle(handle);
}

int main(int argc, char **argv)
{
	int num_cpus;
	int i ;

	num_cpus = get_num_cpus();
	assert(num_cpus >= 1);

	num_devices = ps_list_devices(devices);
	if (num_devices == -1) {
		perror("ps_list_devices");
		exit(1);
	}

	parse_opt(argc, argv);

	for (i = 0; i < num_cpus; i++) {
		int ret = fork();
		assert(ret >= 0);

		my_cpu = i;

		if (ret == 0) {
			bind_cpu(i);
			signal(SIGINT, handle_signal);

			echo();
			return 0;
		}
	}

	signal(SIGINT, SIG_IGN);

	while (1) {
		int ret = wait(NULL);
		if (ret == -1 && errno == ECHILD)
			break;
	}

	return 0;
}
