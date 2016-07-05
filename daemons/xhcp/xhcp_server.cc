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
#include "xhcp_beacon.hh"

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
	char myDAG[MAX_DAG_SIZE];
	char gw_router_4id[MAX_XID_SIZE];
	bool rendezvous = false;

	struct sockaddr_in xianetjoin_addr;
	xianetjoin_addr.sin_family = AF_INET;
	xianetjoin_addr.sin_addr.s_addr = INADDR_ANY;
	xianetjoin_addr.sin_port = htons(XIANETJOIN_ELEMENT_PORT);

	config(argc, argv);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

	// Xsocket init
	int sockfd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		syslog(LOG_ERR, "Unable to create a socket");
		exit(-1);
	}

	// NetJoin socket
	int netjoinfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (netjoinfd < 0) {
		syslog(LOG_ERR, "Unable to create netjoin socket");
		exit(-1);
	}

	// Read the localhost DAG and 4ID
	if ( XreadLocalHostAddr(sockfd, myDAG, MAX_DAG_SIZE, gw_router_4id, MAX_XID_SIZE) < 0 ) {
		syslog(LOG_ERR, "unable to get local address");
		exit(-1);
	}

	// Read the name server DAG from xia-core/etc/resolv.conf
	char ns[XHCP_MAX_DAG_LENGTH];
	char root[BUF_SIZE];
	int rc;
	memset(root, 0, BUF_SIZE);
	rc = ini_gets(NULL, "nameserver", ns, ns, XHCP_MAX_DAG_LENGTH, strcat(XrootDir(root, BUF_SIZE), RESOLV_CONF));
	if(rc <= 0) {
		syslog(LOG_ERR, "ERROR: nameserver not configured in resolv.conf");
		exit(-1);
	}

	// TODO: Remove? Not used - Nitin
	Graph gns(ns);
	gns.fill_sockaddr(&ns_dag);

	// Tell click where the nameserver is so apps on the router can find it
	int sk = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (sk >= 0) {
		XupdateNameServerDAG(sk, ns);
		Xclose(sk);
	} else {
		syslog(LOG_WARNING, "Unable to create socket: %s", strerror(sk));
	}

	// See if a rendezvous control address is configured
	char rv_control_plane_dag[XIA_MAX_DAG_STR_SIZE];
	if(XreadRVServerControlAddr(rv_control_plane_dag, XIA_MAX_DAG_STR_SIZE) == 0) {
		rendezvous = true;
	}

	// Broadcast DAG to send beacons out
	Graph g = Node() * Node(BHID) * Node(SID_XHCP);
	g.fill_sockaddr(&ddag);

	// Build the beacon to send out
	XHCPBeacon beacon;
	beacon.setRouterDAG(myDAG);
	beacon.setRouter4ID(gw_router_4id);
	beacon.setNameServerDAG(ns);
	// Note: beacon will have "" or a complete rendezvous dag
	if (rendezvous) {
		beacon.setRendezvousControlDAG(rv_control_plane_dag);
	}

	while (1) {
		int rc;

		// construct packet
		memset(pkt, 0, sizeof(pkt));
		strcpy(pkt, beacon.to_string().c_str());
		int pktlen = beacon.to_string().size() + 1;

		// send out packet
		//rc = Xsendto(sockfd, pkt, pktlen, 0, (struct sockaddr*)&ddag, sizeof(ddag));
		rc = sendto(netjoinfd, pkt, pktlen, 0, (struct sockaddr*)&xianetjoin_addr, sizeof(xianetjoin_addr));
		if (rc < 0)
			syslog(LOG_WARNING, "Error sending beacon: %s", strerror(rc));
		sleep(XHCP_SERVER_BEACON_INTERVAL);
	}

	return 0;
}
