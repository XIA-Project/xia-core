#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>
#include <syslog.h>

#include "minIni.h"
#include "Xsocket.h"
#include "xhcp.hh"
#include "dagaddr.hpp"

#define DEFAULT_NAME "host0"
#define APPNAME "xhcp_server"

char *hostname = NULL;
char *ident = NULL;


void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-c config] [-h hostname]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)");
	printf(" -v          : log to the console as well as syslog");
	printf(" -h hostname : click device name (default=host0)\n");
	printf("\n");
	exit(0);
}

void config(int argc, char** argv)
{
	int c;
	unsigned level = 3;
	int verbose = 0;

	opterr = 0;

	while ((c = getopt(argc, argv, "h:l:v")) != -1) {
		switch (c) {
			case 'h':
				hostname = strdup(optarg);
				break;
			case 'l':
				level = MIN(atoi(optarg), LOG_DEBUG);
				break;
			case 'v':
				verbose = LOG_PERROR;
				break;
			case '?':
			default:
				// Help Me!
				help(basename(argv[0]));
				break;
		}
	}

	if (!hostname)
		hostname = strdup(DEFAULT_NAME);

	// load the config setting for this hostname
	set_conf("xsockconf.ini", hostname);

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

int main(int argc, char *argv[]) {
	sockaddr_x ddag;
	sockaddr_x ns_dag;
	char pkt[XHCP_MAX_PACKET_SIZE];
	char myAD[MAX_XID_SIZE];
	char gw_router_hid[MAX_XID_SIZE];
	char gw_router_4id[MAX_XID_SIZE];

	config(argc, argv);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

	// Xsocket init
	int sockfd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		syslog(LOG_ERR, "Unable to create a socket");
		exit(-1);
	}

	// dag init
	Graph g = Node() * Node(BHID) * Node(SID_XHCP);
	g.fill_sockaddr(&ddag);

	// read the localhost AD and HID (currently, XHCP server running on gw router)
	if ( XreadLocalHostAddr(sockfd, myAD, MAX_XID_SIZE, gw_router_hid, MAX_XID_SIZE, gw_router_4id, MAX_XID_SIZE) < 0 ) {
		syslog(LOG_ERR, "unable to get local address");
		exit(-1);
	}

	// set the default name server DAG
	char ns[XHCP_MAX_DAG_LENGTH];
	sprintf(ns, "RE %s %s %s", AD0, HID0, SID_NS);

	// read the name server DAG from xia-core/etc/resolv.conf, if present
	char root[BUF_SIZE];
	memset(root, 0, BUF_SIZE);
	ini_gets(NULL, "nameserver", ns, ns, XHCP_MAX_DAG_LENGTH, strcat(XrootDir(root, BUF_SIZE), RESOLV_CONF));

	Graph gns(ns);
	gns.fill_sockaddr(&ns_dag);

	// tell click where the nameserver is so apps on the router can find it
	int sk = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (sk >= 0) {
		XupdateNameServerDAG(sk, ns);
		Xclose(sk);
	} else {
		syslog(LOG_WARNING, "Unable to create socket: %s", strerror(sk));
	}

	xhcp_pkt beacon_pkt;
	beacon_pkt.seq_num = 0;
	beacon_pkt.num_entries = 4;
	xhcp_pkt_entry *ad_entry = (xhcp_pkt_entry*)malloc(sizeof(short)+strlen(myAD)+1);
	xhcp_pkt_entry *gw_entry = (xhcp_pkt_entry*)malloc(sizeof(short)+strlen(gw_router_hid)+1);
	xhcp_pkt_entry *gw_entry_4id = (xhcp_pkt_entry*)malloc(sizeof(short)+strlen(gw_router_4id)+1);
	xhcp_pkt_entry *ns_entry = (xhcp_pkt_entry*)malloc(sizeof(short)+strlen(ns)+1);

	ad_entry->type = XHCP_TYPE_AD;
	gw_entry->type = XHCP_TYPE_GATEWAY_ROUTER_HID;
	gw_entry_4id->type = XHCP_TYPE_GATEWAY_ROUTER_4ID;
	ns_entry->type = XHCP_TYPE_NAME_SERVER_DAG;
	sprintf(ad_entry->data, "%s", myAD);
	sprintf(gw_entry->data, "%s", gw_router_hid);
	sprintf(gw_entry_4id->data, "%s", gw_router_4id);
	sprintf(ns_entry->data, "%s", ns);

	while (1) {
		int rc;

		// construct packet
		memset(pkt, 0, sizeof(pkt));
		int offset = 0;
		memcpy(pkt, &beacon_pkt, sizeof(beacon_pkt.seq_num)+sizeof(beacon_pkt.num_entries));
		offset += sizeof(beacon_pkt.seq_num)+sizeof(beacon_pkt.num_entries);
		memcpy(pkt+offset, &ad_entry->type, sizeof(ad_entry->type));
		offset += sizeof(ad_entry->type);
		memcpy(pkt+offset, ad_entry->data, strlen(ad_entry->data)+1);
		offset += strlen(ad_entry->data)+1;
		memcpy(pkt+offset, &gw_entry->type, sizeof(gw_entry->type));
		offset += sizeof(gw_entry->type);
		memcpy(pkt+offset, gw_entry->data, strlen(gw_entry->data)+1);
		offset += strlen(gw_entry->data)+1;
		memcpy(pkt+offset, &gw_entry_4id->type, sizeof(gw_entry_4id->type));
		offset += sizeof(gw_entry_4id->type);
		memcpy(pkt+offset, gw_entry_4id->data, strlen(gw_entry_4id->data)+1);
		offset += strlen(gw_entry_4id->data)+1;
		memcpy(pkt+offset, &ns_entry->type, sizeof(ns_entry->type));
		offset += sizeof(ns_entry->type);
		memcpy(pkt+offset, ns_entry->data, strlen(ns_entry->data)+1);
		offset += strlen(ns_entry->data)+1;
		// send out packet
		rc = Xsendto(sockfd, pkt, offset, 0, (struct sockaddr*)&ddag, sizeof(ddag));
		if (rc < 0)
			syslog(LOG_WARNING, "Error sending beacon: %s", strerror(rc));
		//fprintf(stderr, "XHCP beacon %ld\n", beacon_pkt.seq_num);
		beacon_pkt.seq_num += 1;
		sleep(XHCP_SERVER_BEACON_INTERVAL);
	}
	free(ad_entry);
	free(gw_entry);
	free(gw_entry_4id);
	free(ns_entry);

	return 0;
}
