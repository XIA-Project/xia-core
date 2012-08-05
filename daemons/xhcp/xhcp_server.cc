#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "minIni.h"
#include "Xsocket.h"
#include "xhcp.hh"

int main(int argc, char *argv[]) {
	char pkt[XHCP_MAX_PACKET_SIZE];
	char ddag[XHCP_MAX_DAG_LENGTH];
	char myAD[MAX_XID_SIZE]; 
	char gw_router_hid[MAX_XID_SIZE];
	char gw_router_4id[MAX_XID_SIZE];
	char ns_dag[XHCP_MAX_DAG_LENGTH];


	if ( argc == 1 ) {
		// Use deault API configuration
	} else if ( argc == 2 ) {
		set_conf("xsockconf.ini", argv[1]);
	} else {
		printf("Expceted usage: xhcp_server [<API conf name>]\n");
	}


	// Xsocket init
	int sockfd = Xsocket(XSOCK_DGRAM);
	if (sockfd < 0) { perror("Opening Xsocket"); }
	// dag init
	sprintf(ddag, "RE %s %s", BHID, SID_XHCP);
	
    	// read the localhost AD and HID (currently, XHCP server running on gw router)
    	if ( XreadLocalHostAddr(sockfd, myAD, MAX_XID_SIZE, gw_router_hid, MAX_XID_SIZE, gw_router_4id, MAX_XID_SIZE) < 0 )
    		perror("Reading localhost address");

        // set the default name server DAG   	
        sprintf(ns_dag, "RE ( %s ) %s %s %s", IP_NS, AD0, HID0, SID_NS);  	

	// read the name server DAG from xia-core/etc/resolv.conf, if present
	ini_gets(NULL, "nameserver", ns_dag, ns_dag, XHCP_MAX_DAG_LENGTH, "../../etc/resolv.conf");
 
 
	xhcp_pkt beacon_pkt;
	beacon_pkt.seq_num = 0;
	beacon_pkt.num_entries = 4;
	xhcp_pkt_entry *ad_entry = (xhcp_pkt_entry*)malloc(sizeof(short)+strlen(myAD)+1);
	xhcp_pkt_entry *gw_entry = (xhcp_pkt_entry*)malloc(sizeof(short)+strlen(gw_router_hid)+1);
	xhcp_pkt_entry *gw_entry_4id = (xhcp_pkt_entry*)malloc(sizeof(short)+strlen(gw_router_4id)+1);
	xhcp_pkt_entry *ns_entry = (xhcp_pkt_entry*)malloc(sizeof(short)+strlen(ns_dag)+1);
		
	ad_entry->type = XHCP_TYPE_AD;
	gw_entry->type = XHCP_TYPE_GATEWAY_ROUTER_HID;
	gw_entry_4id->type = XHCP_TYPE_GATEWAY_ROUTER_4ID;
	ns_entry->type = XHCP_TYPE_NAME_SERVER_DAG;
	sprintf(ad_entry->data, "%s", myAD);
	sprintf(gw_entry->data, "%s", gw_router_hid);
	sprintf(gw_entry_4id->data, "%s", gw_router_4id);
	sprintf(ns_entry->data, "%s", ns_dag);

	while (1) {
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
		Xsendto(sockfd, pkt, offset, 0, ddag, strlen(ddag)+1);
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
