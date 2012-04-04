#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "Xsocket.h"
#include "xhcp.hh"
#include "XIARouter.hh"

unsigned int quit_flag = 0;
char *adv_selfdag = NULL;
char *adv_gwdag = NULL;
char *adv_nsdag = NULL;


XIARouter xr;

void listRoutes(std::string xidType)
{
	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			printf("%s: %d : %s : %ld\n", r.xid.c_str(), r.port, r.nextHop.c_str(), r.flags);
		}
	} else if (rc == 0) {
		printf("No routes exist for %s\n", xidType.c_str());
	} else {
		printf("Error getting route list %d\n", rc);
	}
}

int main(int argc, char *argv[]) {
	int rc;
	pthread_t advertiser_pid;
	char pkt[XHCP_MAX_PACKET_SIZE];
	char sdag[XHCP_MAX_DAG_LENGTH];
	char ddag[XHCP_MAX_DAG_LENGTH];
	char buffer[XHCP_MAX_PACKET_SIZE]; 	
	string myAD(""), myGWRHID(""), myNS_DAG("");
	string default_AD("AD:-"), default_HID("HID:-"), empty_str("HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
	string AD, gwRHID, nsDAG;
	int beacon_reception_count=0;
	int beacon_response_freq = ceil(XHCP_CLIENT_ADVERTISE_INTERVAL/XHCP_SERVER_BEACON_INTERVAL);
	int first_beacon_reception = true;	
	char *self_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *gw_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *ns_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *pseudo_gw_router_dag; // dag for host_register_message (broadcast message), but only the gw router will accept it
	string host_register_message;
	char mydummyAD[MAX_XID_SIZE];
	char myHID[MAX_XID_SIZE]; 	

    	// make the response message dest DAG (intended destination: gw router who is running the routing process)
    	pseudo_gw_router_dag = (char*)malloc(snprintf(NULL, 0, "RE %s %s", BHID, SID_XROUTE) + 1);
    	sprintf(pseudo_gw_router_dag, "RE %s %s", BHID, SID_XROUTE);	

    	// connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click! (%d)\n", rc);
		return -1;
	}
	
	xr.setRouter("host0");
			
	// Xsocket init
	int sockfd = Xsocket(XSOCK_DGRAM);
	if (sockfd < 0) { error("Opening Xsocket"); }
	
    	// read the localhost HID 
    	if ( XreadLocalHostAddr(sockfd, mydummyAD, myHID) < 0 )
    		error("Reading localhost address");   	

	sprintf(sdag, "RE %s", SID_XHCP);
	Xbind(sockfd, sdag);
	
	/* Main operation:
		1. receive myAD, gateway router HID, and Name-Server-DAG information from XHCP server
		2. update AD table: 
			(myAD, 4, -, -)
			(default, 0, gwRHID, -)
		3. update HID table:
			(gwRHID, 0, gwRHID, -)
			(default, 0, gwRHID, -)	
		4. store Name-Server-DAG information	
	*/
	
	// main looping
	while(!(quit_flag & 2)) {
		// clear out packet
		memset(pkt, 0, sizeof(pkt));
		memset(ddag, 0, sizeof(ddag));
		size_t ddaglen = sizeof(ddag);
		int rc = Xrecvfrom(sockfd, pkt, XHCP_MAX_PACKET_SIZE, 0, ddag, &ddaglen);
		if (rc < 0) { error("recvfrom"); }

		memset(self_dag, '\0', XHCP_MAX_DAG_LENGTH);
		memset(gw_dag, '\0', XHCP_MAX_DAG_LENGTH);
		memset(ns_dag, '\0', XHCP_MAX_DAG_LENGTH);
		int i;
		xhcp_pkt *tmp = (xhcp_pkt *)pkt;
		xhcp_pkt_entry *entry = (xhcp_pkt_entry *)tmp->data;
		for (i=0; i<tmp->num_entries; i++) {
			switch (entry->type) {
				case XHCP_TYPE_AD:
					sprintf(self_dag, "%s", entry->data);
					break;
				case XHCP_TYPE_GATEWAY_ROUTER_HID:
					sprintf(gw_dag, "%s", entry->data);
					break;
				case XHCP_TYPE_NAME_SERVER_DAG:
					sprintf(ns_dag, "%s", entry->data);
					break;					
				default:
					fprintf(stderr, "dafault\n");
					break;
			}
			entry = (xhcp_pkt_entry *)((char *)entry + sizeof(entry->type) + strlen(entry->data) + 1);
		}
		// validate pkt
		if (strlen(self_dag) <= 0 || strlen(gw_dag) <= 0 || strlen(ns_dag) <= 0) {
			continue;
		}
		if (adv_selfdag != NULL && adv_gwdag != NULL && adv_nsdag != NULL) {
			if (!strcmp(adv_selfdag, self_dag) && !strcmp(adv_gwdag, gw_dag) && !strcmp(adv_nsdag, ns_dag)) {
				continue;
			}
		}
		
		AD = self_dag;
		gwRHID = gw_dag;
		nsDAG = ns_dag;
		
		// Check if myAD has been changed
		if(myAD.compare(AD) != 0) {
			// update new AD information
			XupdateAD(sockfd, self_dag);
		
			// delete obsolete myAD entry
			xr.delRoute(myAD);
		
			// update AD table (my AD entry)
			if ((rc = xr.setRoute(AD, 4, empty_str, 0xffff)) != 0)
				printf("error setting route %d\n", rc);			
				
			myAD = AD;
		}

		// Check if myGWRouterHID has been changed
		if(myGWRHID.compare(gwRHID) != 0) {
			// delete obsolete gw-router HID entry
			xr.delRoute(myGWRHID);
			
			// update AD (default entry)
			if ((rc = xr.setRoute(default_AD, 0, gwRHID, 0xffff)) != 0)
				printf("error setting route %d\n", rc);			
			
			// update HID table (default entry)
			if ((rc = xr.setRoute(default_HID, 0, gwRHID, 0xffff)) != 0)
				printf("error setting route %d\n", rc);				
			
			// update HID table (gateway router HID entry)
			if ((rc = xr.setRoute(gwRHID, 0, gwRHID, 0xffff)) != 0)
				printf("error setting route %d\n", rc);
				
			myGWRHID = gwRHID;
		}
		
		// Check if myNS_DAG has been changed
		if(myNS_DAG.compare(nsDAG) != 0) {
			// update new name-server-DAG information
			XupdateNameServerDAG(sockfd, ns_dag);		
			myNS_DAG = nsDAG;
		}				
		
		listRoutes("AD");
		printf("\n");
		listRoutes("HID");
		printf("\n");
		
		beacon_reception_count++;
		if(beacon_reception_count >= beacon_response_freq || first_beacon_reception) {
			// construct a registration message to gw router
			/* Message format (delimiter=^)
				message-type{HostRegister = 2}
				host-HID
			*/
			bzero(buffer, XHCP_MAX_PACKET_SIZE);
			host_register_message.clear();
			host_register_message.append("2^");
			host_register_message.append(myHID);
			host_register_message.append("^");
			strcpy (buffer, host_register_message.c_str());
			// send the registraton message to gw router
			Xsendto(sockfd, buffer, strlen(buffer), 0, pseudo_gw_router_dag, strlen(pseudo_gw_router_dag)+1);
			first_beacon_reception = false;
			beacon_reception_count= 0;
		}		
	}	
	return 0;
}

