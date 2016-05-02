#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
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

map<std::string, std::string> name_to_dag_db_table; // map name to dag

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

	// load the config setting for this hostname
	set_conf("xsockconf.ini", hostname);

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s", APPNAME);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

// return true if the AD has an edge that points to the HID
int check_pair(Graph &g, int ad, int hid)
{
	bool found = false;

	// what to do if AD has edges to multiple HIDs?

	if (ad >= 0 && hid >= 0) {
		std::vector<std::size_t> edges = g.get_out_edges(ad);
		std::vector<std::size_t>::iterator ei;
		for (ei = edges.begin(); ei < edges.end(); ei++) {
			if ((int)*ei == hid) {
				found = true;
				break;
			}
		}
	}

	return found;
}

// Called when a name registration is recieved with the migrate flag set.
// This will happen when the xhcp client registers a name for its AD:HID
//  and will happen whenever the host finds itself in a new AD.
// If an older host entry is found for this HID, replace the entry, and
//  find any other names that use the old AD and update them to use the new one
//  instead.
void migrate(const char *name, const char *dag)
{
	map<std::string, std::string>::iterator it;

	it = name_to_dag_db_table.find(name);
	if (it != name_to_dag_db_table.end()) {

		// name is already registered, we may need to migrate it

		string dag_str = it->second;

		Graph new_dag(dag);
		Graph old_dag(dag_str);
		Node *new_ad = NULL;
		Node *new_hid = NULL;
		Node *old_ad = NULL;

		int ad_index = -1;
		int hid_index = -1;

		// find AD & HID in the new DAG
		for (int i = 0; i < new_dag.num_nodes(); i++) {
			Node n = new_dag.get_node(i);

			if (n.type() == Node::XID_TYPE_AD) {
				ad_index = i;
				new_ad = new Node(n);

			} else if (n.type() == Node::XID_TYPE_HID) {
				hid_index = i;
				new_hid = new Node(n);
			}
		}

		if (check_pair(new_dag, ad_index, hid_index)) {

			// we found the new AD/HID and AD has an out edge to HID

			ad_index = -1;
			hid_index = -1;

			// this assumes the old DAG only has a single AD and HID in it
			// and will only reliably work for and dag like AD->HID or IP->(AD->HID)
			for (int i = 0; i < old_dag.num_nodes(); i++) {

				Node n = old_dag.get_node(i);
				if (n.type() == Node::XID_TYPE_AD) {
					ad_index = i;
					old_ad = new Node(n);
				}
				else if (n.type() == Node::XID_TYPE_HID && new_hid->equal_to(n)) {
					// we found an HID that matches the one in the new dag
					hid_index = i;
				}
			}

			if (check_pair(old_dag, ad_index, hid_index)) {

				// HIDs match, time to update dags in our database

				// this dag has migrated, put new record in table
				syslog(LOG_INFO, "migrated host record %s to %s:%s", name,
						new_ad->type_string().c_str(), new_ad->id_string().c_str());
				name_to_dag_db_table[name] = dag;

				// now walk the list of registered dags and update those that should migrate
				/*
				for (it = name_to_dag_db_table.begin(); it != name_to_dag_db_table.end(); it++) {
					string ds = it->second;
					string nm = it->first;
					Graph g(ds);

					ad_index = -1;
					hid_index = -1;

					for (int i = 0; i < g.num_nodes(); i++) {
						if (old_ad->equal_to(g.get_node(i)))
							ad_index = i;
						else if (new_hid->equal_to(g.get_node(i)))
							hid_index = i;
					}

					if (check_pair(g, ad_index, hid_index)) {

						// update the AD node
						syslog(LOG_INFO, "migrated name record %s to %s:%s", nm.c_str(),
							new_ad->type_string().c_str(), new_ad->id_string().c_str());
						g.replace_node_at(ad_index, *new_ad);
						name_to_dag_db_table[nm] = g.dag_string();
					}
				}
				*/
			}
		}

		if (new_ad)  delete new_ad;
		if (new_hid) delete new_hid;
		if (old_ad)  delete old_ad;

	} else {
		syslog(LOG_DEBUG, "%s is new, no migration needed", name);
		syslog(LOG_INFO, "registered %s", name);
		name_to_dag_db_table[name] = dag;
	}
}


int main(int argc, char *argv[]) {
	sockaddr_x ddag;
	int rtype = NS_TYPE_RESPONSE_ERROR;

	char pkt_out[NS_MAX_PACKET_SIZE];
	char pkt_in[NS_MAX_PACKET_SIZE];
	string response_str;

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

    	switch (req_pkt.type) {
		case NS_TYPE_REGISTER:
			// insert a new entry

			if (req_pkt.flags & NS_FLAGS_MIGRATE) {
				// this should be a host record, if no matching name is in the
				//  database, just add the record
				// if the name already exists, check that the HIDs match, and
				//  if so replace the entry, and update the AD in any name records that
				//  contain the same HID
				migrate(req_pkt.name, req_pkt.dag);
			} else {
				// just add the new name record
				syslog(LOG_INFO, "new entry: %s = %s", req_pkt.name, req_pkt.dag);
				name_to_dag_db_table[req_pkt.name] = req_pkt.dag;
			}
			rtype = NS_TYPE_RESPONSE_REGISTER;
			break;

		case NS_TYPE_QUERY:
		{
			map<std::string, std::string>::iterator it;
			it = name_to_dag_db_table.find(req_pkt.name);

			if(it != name_to_dag_db_table.end()) {
				response_str = it->second;
				rtype = NS_TYPE_RESPONSE_QUERY;
				syslog(LOG_DEBUG, "Successful name lookup for %s", req_pkt.name);
			} else {
				rtype = NS_TYPE_RESPONSE_ERROR;
				syslog(LOG_DEBUG, "DAG for %s not found", req_pkt.name);
			}
			break;
		}

		case NS_TYPE_RQUERY:
		{
			map<std::string, std::string>::iterator it;
			// Walk the table and look for a matching DAG
			for(it=name_to_dag_db_table.begin(); it!=name_to_dag_db_table.end(); it++) {
				if(strcmp(it->second.c_str(), req_pkt.dag) != 0) {
					continue;
				}
				response_str = it->first;
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
		response_pkt.flags = 0;
		response_pkt.name = (rtype == NS_TYPE_RESPONSE_RQUERY) ? response_str.c_str(): NULL;
		response_pkt.dag = (rtype == NS_TYPE_RESPONSE_QUERY) ? response_str.c_str() : NULL;
		// pack it up to go on the wire
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
