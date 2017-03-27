#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <algorithm>    // std::find
#include <pthread.h>
#include <string>
#include <string.h>
#include <syslog.h>
#include <stddef.h>
#include <errno.h>
#include <libgen.h>
#include <map>
using namespace std;

#include "Xsocket.h"
#include "xns.h"
#include "dagaddr.hpp"

#define DEFAULT_NAME "host0"
#define APPNAME "xnameservice"

typedef struct {
	char flags;
	std::string dag;
} dbreq;

map<std::string, dbreq> name_to_dag_db_table; // map name to dag

map<std::string, std::vector<std::string> > name_to_dags_db_table; // map name to dags
map<std::string, unsigned> name_to_rr_pointer;

char *hostname = NULL;
char *ident = NULL;

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-c config] [-h hostname]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf(" -v          : log to the console as well as syslog\n");
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

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s", APPNAME);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}


int main(int argc, char *argv[]) {
	sockaddr_x ddag;
	int rtype = NS_TYPE_RESPONSE_ERROR;
	char flags;

	char pkt_out[NS_MAX_PACKET_SIZE];
	char pkt_in[NS_MAX_PACKET_SIZE];
	string response_str;
	char response_flags;

	config(argc, argv);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

	// Xsocket init
	int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (sock < 0) {
		syslog(LOG_ALERT, "Unable to create a socket");
		exit(-1);
	}

	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_NS, NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to get local address");
		exit(-1);
	}

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	if (Xbind(sock, (struct sockaddr*)sa, sizeof(sockaddr_x)) < 0) {
		Graph g(sa);
		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		exit(-1);
	}

	// main looping
	while(1) {
		memset(pkt_in, 0, sizeof(pkt_in));
		socklen_t ddaglen = sizeof(ddag);
		int rc = Xrecvfrom(sock, pkt_in, NS_MAX_PACKET_SIZE, 0, (struct sockaddr*)&ddag, &ddaglen);
		if (rc < 0) {
			syslog(LOG_WARNING, "error receiving data (%s)", strerror(errno));
			continue;
		}

		ns_pkt req_pkt;
		get_ns_packet(pkt_in, rc, &req_pkt);
		flags = req_pkt.flags;

		switch (req_pkt.type) {
		case NS_TYPE_REGISTER:
		{
			// insert a new entry

			dbreq d;

			d.dag = req_pkt.dag;
			d.flags = flags;

			syslog(LOG_INFO, "new entry: %02x %s = %s", flags, req_pkt.name, req_pkt.dag);

			name_to_dag_db_table[req_pkt.name] = d;
			rtype = NS_TYPE_RESPONSE_REGISTER;
			break;
		}
		case NS_TYPE_ANYCAST_REGISTER:
		{
			syslog(LOG_INFO, "new entry: %s = %s", req_pkt.name, req_pkt.dag);
			if(name_to_dags_db_table[req_pkt.name].size() != 0){
				vector<std::string>::iterator it;
				it = std::find(name_to_dags_db_table[req_pkt.name].begin(), name_to_dags_db_table[req_pkt.name].end(), req_pkt.dag);

				// if we have already register the name -> name mapping
				// don't register again
				if(it == name_to_dags_db_table[req_pkt.name].end()){
					name_to_dags_db_table[req_pkt.name].push_back(req_pkt.dag);
				}
			} else {
				name_to_dags_db_table[req_pkt.name].push_back(req_pkt.dag);
				name_to_rr_pointer[req_pkt.name] = 0;
			}

			rtype = NS_TYPE_ANYCAST_RESPONSE_REGISTER;
			break;
		}
		case NS_TYPE_QUERY:
		{
			map<std::string, dbreq>::iterator it;
			it = name_to_dag_db_table.find(req_pkt.name);

			if(it != name_to_dag_db_table.end()) {
				response_str = it->second.dag;
				response_flags = it->second.flags;

				rtype = NS_TYPE_RESPONSE_QUERY;
				if (response_flags == flags) {
				syslog(LOG_DEBUG, "Successful name lookup for (%02x) %s", flags, req_pkt.name);

				} else {
				syslog(LOG_DEBUG, "Semi-successful name lookup for (%0xx != %02x) %s", flags, response_flags, req_pkt.name);

				}
			} else {
				rtype = NS_TYPE_RESPONSE_ERROR;
				syslog(LOG_DEBUG, "DAG for %s not found", req_pkt.name);
			}
			break;
		}
		case NS_TYPE_ANYCAST_QUERY:
		{
			map<std::string, std::vector<std::string> >::iterator it;
			it = name_to_dags_db_table.find(req_pkt.name);

			if(it != name_to_dags_db_table.end()) {
				unsigned currRRPointer = name_to_rr_pointer[req_pkt.name];

				response_str = it->second[currRRPointer%it->second.size()];
				name_to_rr_pointer[req_pkt.name]++;

				rtype = NS_TYPE_ANYCAST_RESPONSE_QUERY;
				syslog(LOG_DEBUG, "Successful name lookup for %s", req_pkt.name);
			} else {
				rtype = NS_TYPE_ANYCAST_RESPONSE_ERROR;
				syslog(LOG_DEBUG, "DAG for %s not found", req_pkt.name);
			}

			break;
		}
		case NS_TYPE_RQUERY:
		{
			map<std::string, dbreq>::iterator it;
			// Walk the table and look for a matching DAG
			for(it=name_to_dag_db_table.begin(); it!=name_to_dag_db_table.end(); it++) {
				if(it->second.flags != flags || strcmp(it->second.dag.c_str(), req_pkt.dag) != 0) {
					continue;
				}
				response_str = it->first;
				response_flags = flags;
				rtype = NS_TYPE_RESPONSE_RQUERY;
				syslog(LOG_DEBUG, "Successful DAG lookup for %s", req_pkt.dag);
			}
			break;
		}

		default:
			syslog(LOG_WARNING, "unrecognized request: %d", req_pkt.type);
			rtype = NS_TYPE_RESPONSE_ERROR;
			break;
		}

		//Construct a response packet
		ns_pkt response_pkt;

		response_pkt.type = rtype;
		response_pkt.flags = flags;
		response_pkt.name = (rtype == NS_TYPE_RESPONSE_RQUERY) ? response_str.c_str(): NULL;
		response_pkt.dag = (rtype == NS_TYPE_RESPONSE_QUERY || rtype == NS_TYPE_ANYCAST_RESPONSE_QUERY) ? response_str.c_str() : NULL;		// pack it up to go on the wire
		int len = make_ns_packet(&response_pkt, pkt_out, sizeof(pkt_out));

		//Send the response packet back to the query node
		rc = Xsendto(sock, pkt_out, len, 0, (struct sockaddr*)&ddag, sizeof(ddag));
		if (rc >= 0)
			syslog(LOG_DEBUG, "returned %d bytes", rc);
		else
			syslog(LOG_WARNING, "unable to send response (%d)", errno);
	}
	return 0;
}
