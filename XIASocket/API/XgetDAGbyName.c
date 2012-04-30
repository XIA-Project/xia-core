/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
 @file XgetDAGbyName.c
 @brief Implements XgetDAGbyName() and XregisterName
*/
#include <errno.h>
#include "Xsocket.h"
#include "Xinit.h"
#include "Xutil.h"

#define NS_MAX_PACKET_SIZE 1024
#define NS_MAX_DAG_LENGTH 1024

#define NS_TYPE_REGISTER 1
#define NS_TYPE_QUERY 2
#define NS_TYPE_RESPONSE 3
#define NS_TYPE_RESPONSE_ERROR 4

typedef struct ns_pkt {
	short type;
	char* name;
	char* dag;
} ns_pkt;


/*!
** @brief Lookup a DAG based using a host or service name.
**
** The name should be a string such as www_s.example.xia or host.example.xia. 
** By convention services are indicated by '_s' appended to the service name.
** The memory returned is dynamically allocated and should be released with a
** call to free() when the caller is done with it.
**
** This is a very simple implementation of the name query function.
** It will be replaces in a future release.
**
** @param name The name of an XIA service or host. 
**
** @returns a character point to the dag on success
** @returns NULL on failure
**
*/
char *XgetDAGbyName(const char *name) {
    int sock;
    char pkt[NS_MAX_PACKET_SIZE],ddag[NS_MAX_PACKET_SIZE], ns_dag[NS_MAX_PACKET_SIZE]; 
    char *dag; 
    char _name[NS_MAX_DAG_LENGTH], _dag[NS_MAX_DAG_LENGTH];      
    int result;
    
    //Open socket
    sock=Xsocket(XSOCK_DGRAM);
    
    //Read the nameserver DAG (the one that the name-query will be sent to)
    if ( XreadNameServerDAG(sock, ns_dag) < 0 )
    	error("Reading nameserver address");

    //Construct a name-query packet	
    ns_pkt query_pkt;
    query_pkt.type = NS_TYPE_QUERY;
    query_pkt.name = (char*)malloc(strlen(name)+1);
    sprintf(query_pkt.name, "%s", name);
    //query_pkt.dag = (char*)malloc(strlen(DAG)+1);
    //sprintf(query_pkt.dag, "%s", DAG);

    memset(pkt, 0, sizeof(pkt));
    int offset = 0;
    memcpy(pkt, &query_pkt.type, sizeof(query_pkt.type));
    offset += sizeof(query_pkt.type);
    memcpy(pkt+offset, query_pkt.name, strlen(query_pkt.name)+1);
    offset += strlen(query_pkt.name)+1;
    //memcpy(pkt+offset, query_pkt.dag, strlen(query_pkt.dag)+1);
    //offset += strlen(query_pkt.dag)+1;
 
    //Send a name query to the name server	
    Xsendto(sock, pkt, offset, 0, ns_dag, strlen(ns_dag)+1);
    
    //Check the response from the name server
    memset(pkt, 0, sizeof(pkt));
    memset(ddag, 0, sizeof(ddag));
    size_t ddaglen = sizeof(ddag);
    int rc = Xrecvfrom(sock, pkt, NS_MAX_PACKET_SIZE, 0, ddag, &ddaglen);
    if (rc < 0) { error("recvfrom"); }

    memset(_name, '\0', NS_MAX_DAG_LENGTH);
    memset(_dag, '\0', NS_MAX_DAG_LENGTH);
   
    ns_pkt *tmp = (ns_pkt *)pkt;
    char* tmp_name = (char*)(pkt+sizeof(tmp->type));
    char* tmp_dag = (char*)(pkt+sizeof(tmp->type)+ strlen(tmp_name)+1);
    switch (tmp->type) {
	case NS_TYPE_RESPONSE:
		sprintf(_name, "%s", tmp_name);
		sprintf(_dag, "%s", tmp_dag);
		result = 1;
		break;
	case NS_TYPE_RESPONSE_ERROR:
		result = -1;
		break;					
	default:
		fprintf(stderr, "dafault\n");
		result = -1;
		break;
    }	        
    if (result < 0) {
    	return NULL;
    }  
    dag = (char*)malloc(sizeof(_dag) + 1);
    strcpy(dag, _dag);
    
    //Close socket	
    Xclose(sock);
    free(query_pkt.name);  
        
    return dag;
}


/*!
** @brief Register a service or hostname with the name server.
**
** Register a host or service name with the XIA nameserver.
** By convention services are indicated by '_s' appended to the service name.
** The memory returned is dynamically allocated and should be released with a
** call to free() when the caller is done with it.
**
** This is a very simple implementation and will be replaced in a 
** future release. This version does not check correctness of the name or dag,
** nor does it check to ensure that the client is allowed to bind to name.
**
** @param name - The name of an XIA service or host.
** @param DAG  - the DAG to be bound to name.
**
** @returns 0 on success
** @returns -1 on failure with errno set
**
*/
int XregisterName(const char *name, const char *DAG) {
    int sock;
    char pkt[NS_MAX_PACKET_SIZE],ddag[NS_MAX_PACKET_SIZE], ns_dag[NS_MAX_PACKET_SIZE];  
    char _name[NS_MAX_DAG_LENGTH], _dag[NS_MAX_DAG_LENGTH];    
    int result;

    //Open socket
    sock=Xsocket(XSOCK_DGRAM);
    
    //Read the nameserver DAG (the one that the name-registration will be sent to)
    if ( XreadNameServerDAG(sock, ns_dag) < 0 )
    	error("Reading nameserver address");	

    //Construct a registration packet	
    ns_pkt register_pkt;
    register_pkt.type = NS_TYPE_REGISTER;
    register_pkt.name = (char*)malloc(strlen(name)+1);
    register_pkt.dag = (char*)malloc(strlen(DAG)+1);
    sprintf(register_pkt.name, "%s", name);
    sprintf(register_pkt.dag, "%s", DAG);

    memset(pkt, 0, sizeof(pkt));
    int offset = 0;
    memcpy(pkt, &register_pkt.type, sizeof(register_pkt.type));
    offset += sizeof(register_pkt.type);
    memcpy(pkt+offset, register_pkt.name, strlen(register_pkt.name)+1);
    offset += strlen(register_pkt.name)+1;
    memcpy(pkt+offset, register_pkt.dag, strlen(register_pkt.dag)+1);
    offset += strlen(register_pkt.dag)+1;       

    //Send the name registration packet to the name server	
    Xsendto(sock, pkt, offset, 0, ns_dag, strlen(ns_dag)+1);

    //Check the response from the name server
    memset(pkt, 0, sizeof(pkt));
    memset(ddag, 0, sizeof(ddag));
    size_t ddaglen = sizeof(ddag);
    int rc = Xrecvfrom(sock, pkt, NS_MAX_PACKET_SIZE, 0, ddag, &ddaglen);
    if (rc < 0) { error("recvfrom"); }

    memset(_name, '\0', NS_MAX_DAG_LENGTH);
    memset(_dag, '\0', NS_MAX_DAG_LENGTH);
   
    ns_pkt *tmp = (ns_pkt *)pkt;
    char* tmp_name = (char*)(pkt+sizeof(tmp->type));
    char* tmp_dag = (char*)(pkt+sizeof(tmp->type)+ strlen(tmp_name)+1);
    switch (tmp->type) {
	case NS_TYPE_RESPONSE:
		sprintf(_name, "%s", tmp_name);
		sprintf(_dag, "%s", tmp_dag);
		result = 0;
		break;
	case NS_TYPE_RESPONSE_ERROR:
		result = -1;
		break;					
	default:
		fprintf(stderr, "dafault\n");
		result = -1;
		break;
     }	        
    free(register_pkt.name);
    free(register_pkt.dag);
    
    //Close socket	
    Xclose(sock);
    
    return result;
}
	
