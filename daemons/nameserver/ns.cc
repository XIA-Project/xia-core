#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <string>
#include <string.h>
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

typedef struct ns_pkt {
	short type;
	char* name;
	char* dag;
} ns_pkt;


int main(int argc, char *argv[]) {
	UNUSED(argc);
	UNUSED(argv);

	sockaddr_x ddag;

	char pkt[NS_MAX_PACKET_SIZE];
	char _name[NS_MAX_DAG_LENGTH], _dag[NS_MAX_DAG_LENGTH]; 
	string name_str, dag_str;
	map<std::string, std::string> name_to_dag_db_table; // map name to dag 
	bool _error; // check if any error while processing query/register

	// Xsocket init
	int sock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (sock < 0) { perror("Opening Xsocket"); }
	
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_NS, NULL, &ai) != 0) {
		perror("getaddrinfo failure!\n");
		exit(-1);
	}

	sockaddr_x *sa = (sockaddr_x*)ai->ai_addr;

	if (Xbind(sock, (struct sockaddr*)sa, sizeof(sockaddr_x)) < 0) {
		perror("unable to bind to the socket");
		exit(-1);
	}
	
	// main looping
	while(1) {
		memset(pkt, 0, sizeof(pkt));
		socklen_t ddaglen = sizeof(ddag);
		int rc = Xrecvfrom(sock, pkt, NS_MAX_PACKET_SIZE, 0, (struct sockaddr*)&ddag, &ddaglen);
		if (rc < 0) { perror("recvfrom"); }

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
			} else {
				dag_str = "not found";
				_error = true;
			}
				
		}
			break;					
		default:
			fprintf(stderr, "dafault\n");
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
		Xsendto(sock, pkt, offset, 0, (struct sockaddr*)&ddag, sizeof(ddag));
	}
	return 0;
}

