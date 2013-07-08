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
#include <errno.h>
#include <libgen.h>
#include <map>
using namespace std;

#include "Xsocket.h"
#include "dagaddr.hpp"

#define NS_MAX_PACKET_SIZE 1024
#define NS_MAX_DAG_LENGTH 1024

#define NS_TYPE_REGISTER 1
#define NS_TYPE_QUERY 2
#define NS_TYPE_RESPONSE 3
#define NS_TYPE_RESPONSE_ERROR 4

//#define SID_XHCP "SID:1110000000000000000000000000000000001111"
//#define SID_XROUTE "SID:1110000000000000000000000000000000001112"
#define SID_NS "SID:1110000000000000000000000000000000001113"

#define DEFAULT_NAME "host0"
#define APPNAME "xnameservice"

char *hostname = NULL;
char *ident = NULL;

typedef struct ns_pkt {
	short type;
	char* name;
	char* dag;
} ns_pkt;

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-c config] [-h nostname]\n", name);
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
	ident = (char *)calloc(strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s", APPNAME);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

int main(int argc, char *argv[]) {
	sockaddr_x ddag;

	char pkt[NS_MAX_PACKET_SIZE];
	char _name[NS_MAX_DAG_LENGTH], _dag[NS_MAX_DAG_LENGTH];
	string name_str, dag_str;
	map<std::string, std::string> name_to_dag_db_table; // map name to dag
	bool _error; // check if any error while processing query/register

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
		memset(pkt, 0, sizeof(pkt));
		socklen_t ddaglen = sizeof(ddag);
		int rc = Xrecvfrom(sock, pkt, NS_MAX_PACKET_SIZE, 0, (struct sockaddr*)&ddag, &ddaglen);
		if (rc < 0) {
			syslog(LOG_WARNING, "error receiving data (%s)", strerror(errno));
			continue;
		}

		memset(_name, '\0', NS_MAX_DAG_LENGTH);
    	memset(_dag, '\0', NS_MAX_DAG_LENGTH);
   		_error = false;
    	ns_pkt *tmp = (ns_pkt *)pkt;
		char* tmp_name = (char*)(pkt+sizeof(tmp->type));
    	char* tmp_dag = (char*)(pkt+sizeof(tmp->type)+ strlen(tmp_name)+1);
    	switch (tmp->type) {
		case NS_TYPE_REGISTER:
			sprintf(_name, "%s", tmp_name);
			sprintf(_dag, "%s", tmp_dag);
			name_str = _name;
			dag_str = _dag;
			// insert a new entry
			name_to_dag_db_table[name_str] = dag_str;
			syslog(LOG_INFO, "new entry: %s = %s", _name, _dag);
			break;
		case NS_TYPE_QUERY:
		{
			sprintf(_name, "%s", tmp_name);
			name_str = _name;
			// lookup the name table
			map<std::string, std::string>::iterator it;
			it=name_to_dag_db_table.find(name_str);
			if(it != name_to_dag_db_table.end()) {
				dag_str = it->second;
				syslog(LOG_DEBUG, "Successful name lookup for %s", _name);
			} else {
				dag_str = "not found";
				_error = true;
				syslog(LOG_DEBUG, "DAG for %s not found", _name);
			}

		}
			break;
		default:
			syslog(LOG_WARNING, "unrecognized request: %d", tmp->type);
			_error = true;
			break;
		}

		//Construct a response packet
		ns_pkt response_pkt;
		if(_error){
			response_pkt.type = NS_TYPE_RESPONSE_ERROR;
		} else {
			response_pkt.type = NS_TYPE_RESPONSE;
		}
		response_pkt.name = (char*)malloc(strlen(name_str.c_str())+1);
		response_pkt.dag = (char*)malloc(strlen(dag_str.c_str())+1);
		sprintf(response_pkt.name, "%s", name_str.c_str());
		sprintf(response_pkt.dag, "%s", dag_str.c_str());

		memset(pkt, 0, sizeof(pkt));
		int offset = 0;
		memcpy(pkt, &response_pkt.type, sizeof(response_pkt.type));
		offset += sizeof(response_pkt.type);
		memcpy(pkt+offset, response_pkt.name, strlen(response_pkt.name)+1);
		offset += strlen(response_pkt.name)+1;
		memcpy(pkt+offset, response_pkt.dag, strlen(response_pkt.dag)+1);
		offset += strlen(response_pkt.dag)+1;

		//Send the response packet back to the query node
		rc = Xsendto(sock, pkt, offset, 0, (struct sockaddr*)&ddag, sizeof(ddag));
		syslog(LOG_DEBUG, "sent %d characters", rc);
	}
	return 0;
}

