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
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <libgen.h>
#include <syslog.h>

#include "Xsocket.h"
#include "xns.h"
#include "xhcp.hh"
#include "../common/XIARouter.hh"
#include "dagaddr.hpp"


unsigned int quit_flag = 0;
char *adv_selfdag = NULL;
char *adv_gwdag = NULL;
char *adv_gw4id = NULL;
char *adv_nsdag = NULL;

#define DEFAULT_NAME "host0"
#define EXTENSION "xia"
#define APPNAME "xhcp_client"

char *hostname = NULL;
char *fullname = NULL;
char *ident = NULL;

XIARouter xr;

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-c config] [-h hostname]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)\n");
	printf(" -v	         : log to the console as well as syslog\n");
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

	if (!hostname) {
		hostname = strdup(DEFAULT_NAME);
	}
	fullname = (char*)calloc(1, strlen(hostname) + strlen(EXTENSION) + 1);
	sprintf(fullname, "%s.%s", hostname, EXTENSION);

	// load the config setting for this hostname
	set_conf("xsockconf.ini", hostname);

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

void listRoutes(std::string xidType)
{
	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			syslog(LOG_INFO, "%s: %d : %s : %ld\n", r.xid.c_str(), r.port, r.nextHop.c_str(), r.flags);
		}
	} else if (rc == 0) {
		syslog(LOG_INFO, "No routes exist for %s\n", xidType.c_str());
	} else {
		syslog(LOG_INFO, "Error getting route list: %d\n", rc);
	}
}

int main(int argc, char *argv[]) {
	int rc;
	sockaddr_x sdag;
	sockaddr_x hdag;
	sockaddr_x ddag;
	char pkt[XHCP_MAX_PACKET_SIZE];
	char buffer[XHCP_MAX_PACKET_SIZE]; 	
	string myAD(""), myGWRHID(""), myGWR4ID(""), myNS_DAG("");
	string default_AD("AD:-"), default_HID("HID:-"), default_4ID("IP:-");
	string empty_str("");
	string AD, gwRHID, gwR4ID, nsDAG;
	unsigned  beacon_reception_count=0;
	unsigned beacon_response_freq = ceil(XHCP_CLIENT_ADVERTISE_INTERVAL/XHCP_SERVER_BEACON_INTERVAL);
	int update_ns = 0;
	char *self_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *gw_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *gw_4id = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *ns_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	sockaddr_x pseudo_gw_router_dag; // dag for host_register_message (broadcast message), but only the gw router will accept it
	string host_register_message;
	char mydummyAD[MAX_XID_SIZE];
	char myrealAD[MAX_XID_SIZE];
	char myHID[MAX_XID_SIZE]; 
	char my4ID[MAX_XID_SIZE];	
	int changed;

	config(argc, argv);
	xr.setRouter(hostname);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

	// make the response message dest DAG (intended destination: gw router who is running the routing process)
	Graph gw = Node() * Node(BHID) * Node(SID_XROUTE);
	gw.fill_sockaddr(&pseudo_gw_router_dag);

	// connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		syslog(LOG_ERR, "unable to connect to click! (%d)\n", rc);
		return -1;
	}
	
	// Xsocket init
	int sockfd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		syslog(LOG_ERR, "unable to create a socket");
		exit(-1);
	}
	
   	// read the localhost HID 
   	if ( XreadLocalHostAddr(sockfd, mydummyAD, MAX_XID_SIZE, myHID, MAX_XID_SIZE, my4ID, MAX_XID_SIZE) < 0 ) {
		syslog(LOG_ERR, "unable to retrieve local host address");
		Xclose(sockfd);
	}

	// make the src DAG (Actual AD will be updated when receiving XHCP beacons from an XHCP server)
	Graph g = Node() * Node(SID_XHCP);
	g.fill_sockaddr(&sdag);

	if (Xbind(sockfd, (struct sockaddr*)&sdag, sizeof(sdag)) < 0) {
		syslog(LOG_ERR, "unable to bind to %s", g.dag_string().c_str());
		Xclose(sockfd);
		exit(-1);
	}
	
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
		socklen_t ddaglen = sizeof(ddag);
		int rc = Xrecvfrom(sockfd, pkt, XHCP_MAX_PACKET_SIZE, 0, (struct sockaddr*)&ddag, &ddaglen);

		if (rc < 0) {
			syslog(LOG_WARNING, "error receiving data");
			continue;
		}
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
					syslog(LOG_WARNING, "invalid xhcp data, discarding...");
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
		
		changed = 0;

		// Check if myAD has been changed
		if(myAD.compare(AD) != 0) {
			changed = 1;
			syslog(LOG_INFO, "AD updated");
			// update new AD information
			if(XupdateAD(sockfd, self_dag, gw_4id) < 0)
				syslog(LOG_WARNING, "Error updating my AD");
		
			// delete obsolete myAD entry
			xr.delRoute(myAD);
		
			// update AD table (my AD entry)
			if ((rc = xr.setRoute(AD, DESTINED_FOR_LOCALHOST, empty_str, 0xffff)) != 0)
				syslog(LOG_WARNING, "error setting route %d\n", rc);
				
			myAD = AD;
			update_ns = 1;
		}

		// Check if myGWRouterHID has been changed
		if(myGWRHID.compare(gwRHID) != 0) {
			changed = 1;
			syslog(LOG_INFO, "default route updated");

			// delete obsolete gw-router HID entry
			xr.delRoute(myGWRHID);
			
			// TODO: These default next-hops shouldn't be hard coded; when we get a message
			// telling us what the router's HID is, we should look through the tables and
			// update the next hop for anything point to port 0.

			// update AD (default entry)
			if ((rc = xr.setRoute(default_AD, 0, gwRHID, 0xffff)) != 0)
				syslog(LOG_WARNING, "error setting route %d\n", rc);
				
			// update 4ID table (default entry)
			if ((rc = xr.setRoute(default_4ID, 0, gwRHID, 0xffff)) != 0)
				syslog(LOG_WARNING, "error setting route %d\n", rc);
			
			// update HID table (default entry)
			if ((rc = xr.setRoute(default_HID, 0, gwRHID, 0xffff)) != 0)
				syslog(LOG_WARNING, "error setting route %d\n", rc);
			
			// update HID table (gateway router HID entry)
			if ((rc = xr.setRoute(gwRHID, 0, gwRHID, 0xffff)) != 0)
				syslog(LOG_WARNING, "error setting route %d\n", rc);
				
			if ((rc = xr.setRoute(default_HID, 0, gwRHID, 0xffff)) != 0)
				syslog(LOG_WARNING, "error setting route %d\n", rc);
				
			myGWRHID = gwRHID;
		}
		
		// Check if myNS_DAG has been changed
		if(myNS_DAG.compare(nsDAG) != 0) {

			// update new name-server-DAG information
			XupdateNameServerDAG(sockfd, ns_dag);		
			myNS_DAG = nsDAG;
		}				
		
		if (changed) {
			listRoutes("AD");
			listRoutes("HID");
		}

		beacon_reception_count++;
		if ((beacon_reception_count % beacon_response_freq) == 0) {
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
			Xsendto(sockfd, buffer, strlen(buffer), 0, (sockaddr*)&pseudo_gw_router_dag, sizeof(pseudo_gw_router_dag));
			// TODO: Hack to allow intrinsic security code to drop packet
			// Ideally there should be a handshake with the gateway router
			sleep(5);
			Xsendto(sockfd, buffer, strlen(buffer), 0, (sockaddr*)&pseudo_gw_router_dag, sizeof(pseudo_gw_router_dag));
		}

		//Register this hostname to the name server
		if (update_ns && beacon_reception_count >= XHCP_CLIENT_NAME_REGISTER_WAIT) {	
			// read the localhost HID 
			if ( XreadLocalHostAddr(sockfd, myrealAD, MAX_XID_SIZE, myHID, MAX_XID_SIZE, my4ID, MAX_XID_SIZE) < 0 ) {
				syslog(LOG_WARNING, "error reading localhost address"); 
				continue;
			}

			// make the host DAG 
			Node n_src = Node();
			Node n_ip  = Node(my4ID);
			Node n_ad  = Node(myrealAD);
			Node n_hid = Node(myHID);

			Graph hg = (n_src * n_ad * n_hid);
			hg = hg + (n_src * n_ip * n_ad * n_hid);
			hg.fill_sockaddr(&hdag);
			if (XregisterHost(fullname, &hdag) < 0 ) {
				syslog(LOG_ERR, "error registering new DAG for %s", fullname);

				// reset beacon counter so we try again in a few seconds
				beacon_reception_count = 0;
			} else {
				syslog(LOG_INFO, "registered %s as %s", fullname, hg.dag_string().c_str());
				update_ns = 0;
			}
		}   
	}	
	return 0;
}

