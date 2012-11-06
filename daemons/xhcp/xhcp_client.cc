#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>

#include "Xsocket.h"
#include "xhcp.hh"
#include "../common/XIARouter.hh"

#define DEFAULT_HOSTNAME "www_h.hostxxx.com.xia" // used if not provided by user (via command-line)

unsigned int quit_flag = 0;
char *adv_selfdag = NULL;
char *adv_gwdag = NULL;
char *adv_gw4id = NULL;
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
	char hdag[XHCP_MAX_DAG_LENGTH];
	char buffer[XHCP_MAX_PACKET_SIZE]; 	
	string myAD(""), myGWRHID(""), myGWR4ID(""), myNS_DAG("");
	string default_AD("AD:-"), default_HID("HID:-"), default_4ID("IP:-"), empty_str("HID:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
	string AD, gwRHID, gwR4ID, nsDAG;
	int beacon_reception_count=0;
	int beacon_response_freq = ceil(XHCP_CLIENT_ADVERTISE_INTERVAL/XHCP_SERVER_BEACON_INTERVAL);
	int first_beacon_reception = true;	
	char *self_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *gw_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *gw_4id = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *ns_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *pseudo_gw_router_dag; // dag for host_register_message (broadcast message), but only the gw router will accept it
	string host_register_message;
	char mydummyAD[MAX_XID_SIZE];
	char myrealAD[MAX_XID_SIZE];
	char myHID[MAX_XID_SIZE]; 
	char my4ID[MAX_XID_SIZE];	
    char *hostname;

 	// set hostname, click element name, and API conf
	if ( argc < 2 ) {
		hostname = (char*) malloc(snprintf(NULL, 0, "%s", DEFAULT_HOSTNAME) + 1);
		sprintf(hostname, "%s", DEFAULT_HOSTNAME);

		xr.setRouter("host0");

    } else if ( argc == 2 || argc == 3 || argc == 4 ) {
		std::string commandline_input = argv[1];
		if (commandline_input.find("www_h.") != string::npos) {	
			hostname = (char*) malloc(snprintf(NULL, 0, "%s", argv[1]) + 1);
		    sprintf(hostname, "%s", argv[1]);
		} else {
			hostname = (char*) malloc(snprintf(NULL, 0, "%s", DEFAULT_HOSTNAME) + 1);
		    sprintf(hostname, "%s", DEFAULT_HOSTNAME);
		}

		if ( argc == 3 || argc == 4 ) {
			char* element_name = (char*)malloc(snprintf(NULL, 0, "%s", argv[2]) + 1);
			sprintf(element_name, "%s", argv[2]);
			xr.setRouter(element_name);
		} else {
			xr.setRouter("host0");
		}
		if ( argc == 4 ) {
			set_conf("xsockconf.ini", argv[3]);
		}
    } else {
		printf("Expected usage: xhcp_client [<hostname> [<element name> [<API conf name>]]]\n");
	}

    // make the response message dest DAG (intended destination: gw router who is running the routing process)
    pseudo_gw_router_dag = (char*)malloc(snprintf(NULL, 0, "RE %s %s", BHID, SID_XROUTE) + 1);
    sprintf(pseudo_gw_router_dag, "RE %s %s", BHID, SID_XROUTE);	

    // connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click! (%d)\n", rc);
		return -1;
	}
	
	// Xsocket init
	int sockfd = Xsocket(XSOCK_DGRAM);
	if (sockfd < 0) { perror("Opening Xsocket"); }
	
    	// read the localhost HID 
    	if ( XreadLocalHostAddr(sockfd, mydummyAD, MAX_XID_SIZE, myHID, MAX_XID_SIZE, my4ID, MAX_XID_SIZE) < 0 )
    		perror("Reading localhost address");   	

	// make the src DAG (Actual AD will be updated when receiving XHCP beacons from an XHCP server)
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
		4. update 4ID table:
			(default, 0, gwRHID, -)				
		5. store Name-Server-DAG information
                6. register hostname to the name server  	
	*/
	
	// main looping
	while(!(quit_flag & 2)) {
		// clear out packet
		memset(pkt, 0, sizeof(pkt));
		memset(ddag, 0, sizeof(ddag));
		size_t ddaglen = sizeof(ddag);
		int rc = Xrecvfrom(sockfd, pkt, XHCP_MAX_PACKET_SIZE, 0, ddag, &ddaglen);
		if (rc < 0) { perror("recvfrom"); }

		memset(self_dag, '\0', XHCP_MAX_DAG_LENGTH);
		memset(gw_dag, '\0', XHCP_MAX_DAG_LENGTH);
		memset(gw_4id, '\0', XHCP_MAX_DAG_LENGTH);
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
				case XHCP_TYPE_GATEWAY_ROUTER_4ID:
					sprintf(gw_4id, "%s", entry->data);
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
		if (strlen(self_dag) <= 0 || strlen(gw_dag) <= 0 || strlen(gw_4id) <= 0 || strlen(ns_dag) <= 0) {
			continue;
		}
		if (adv_selfdag != NULL && adv_gwdag != NULL && adv_nsdag != NULL) {
			if (!strcmp(adv_selfdag, self_dag) && !strcmp(adv_gwdag, gw_dag) && !strcmp(adv_gw4id, gw_4id) && !strcmp(adv_nsdag, ns_dag)) {
				continue;
			}
		}
		
		AD = self_dag;
		gwRHID = gw_dag;
		gwR4ID = gw_4id;
		nsDAG = ns_dag;
		
		// Check if myAD has been changed
		if(myAD.compare(AD) != 0) {
			// update new AD information
			XupdateAD(sockfd, self_dag, gw_4id);
		
			// delete obsolete myAD entry
			xr.delRoute(myAD);
		
			// update AD table (my AD entry)
			if ((rc = xr.setRoute(AD, DESTINED_FOR_LOCALHOST, empty_str, 0xffff)) != 0)
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
				
			// update 4ID table (default entry)
			if ((rc = xr.setRoute(default_4ID, 0, gwRHID, 0xffff)) != 0)
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

		//Register this hostname to the name server
                if (beacon_reception_count == XHCP_CLIENT_NAME_REGISTER_WAIT) {    
    			// read the localhost HID 
    			if ( XreadLocalHostAddr(sockfd, myrealAD, MAX_XID_SIZE, myHID, MAX_XID_SIZE, my4ID, MAX_XID_SIZE) < 0 )
    				perror("Reading localhost address"); 
    			// make the host DAG 
        		sprintf(hdag, "RE ( %s ) %s %s", my4ID, myrealAD, myHID);  
    			if (XregisterName(hostname, hdag) < 0 )
    				perror("name register");
		}   
		
	}	
	return 0;
}

