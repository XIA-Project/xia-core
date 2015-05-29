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
#include "xhcp_beacon.hh"
#include "xhcp_interface.hh"
#include "../common/XIARouter.hh"
#include "dagaddr.hpp"

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

// Setup connection with Click
// Create an Xsocket
// Bind to receive SID_XHCP beacons
// Return: socket descriptor to listen for beacons
int initialize()
{
	int retval = -1;
	int rc;
	int sockfd;
	int state = 0;
	sockaddr_x xhcp_dag;
	Graph g;

	// Set up a connection with Click
	xr.setRouter(hostname);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

	// connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		syslog(LOG_ERR, "unable to connect to click! (%d)\n", rc);
		goto initialize_cleanup;
	}
	state = 1;

    // Xsocket init
    sockfd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "unable to create a socket");
		goto initialize_cleanup;
    }
	state = 2;

	// DAG to listen for XHCP beacons
	g = Node() * Node(SID_XHCP);
	g.fill_sockaddr(&xhcp_dag);

	// Bind socket to listen for beacons
	if (Xbind(sockfd, (struct sockaddr*)&xhcp_dag, sizeof(xhcp_dag)) < 0) {
		syslog(LOG_ERR, "unable to bind to %s", g.dag_string().c_str());
		goto initialize_cleanup;
	}
	
	// All good return socket descriptor
	retval = sockfd;


initialize_cleanup:
	// On error, close all state and return negative value
	if(retval < 0) {
		switch(state) {
			case 2:
				Xclose(sockfd);
			case 1:
				xr.close();
		};
	}
	return retval;
}

// Read a beacon from sockfd and return interface and beacon contents
int get_beacon(int sockfd, int *iface, char *beacon, int beaconlen)
{
	int retval = -1;
	sockaddr_x client;
	struct msghdr msg;
	struct iovec iov;
	struct in_pktinfo pi;

	struct cmsghdr *cmsg;
	struct in_pktinfo *pinfo;

	assert(beaconlen == XIA_MAXBUF);

	msg.msg_name = &client;
	msg.msg_namelen = sizeof(client);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	iov.iov_base = beacon;
	iov.iov_len = XIA_MAXBUF;

	char cbuf[CMSG_SPACE(sizeof pi)];

	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = IPPROTO_IP;
	cmsg->cmsg_type = IP_PKTINFO;
	cmsg->cmsg_len = CMSG_LEN(sizeof(pi));

	if (Xrecvmsg(sockfd, &msg, 0) < 0) {
		printf("error receiving beacon\n");
		retval = -1;
	} else {
		// get the interface it came in on
		for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
			if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
				pinfo = (struct in_pktinfo*) CMSG_DATA(cmsg);
				*iface = pinfo->ipi_ifindex;
				retval = 0;
			}
		}
	}

	if(retval == 0) {
		Graph g(&client);
		printf("beacon on click port %hd from:\n%s\n", *iface, g.dag_string().c_str());
	}
	return retval;
}

int main(int argc, char *argv[]) {
	int sockfd;

	// Parse the arguments
	config(argc, argv);
	// A table with columns: InterfaceNumber, InterfaceName, InterfaceHID, AD, RHID, R4ID, NameServer
	XHCPInterface interfaces[XHCP_MAX_INTERFACES];
	/*
	// A pointer to the default Interface.
	int default_interface = -1;
	*/
	// Receive beacon from an interface.
	if((sockfd = initialize()) < 0) {
		syslog(LOG_ERR, "Failed to set up beacon receiver\n");
		return -1;
	}
	// If AD changes - update XupdateAD(iface, HID, AD, R4ID) - Xtransport updates DAG for iface in XIAInterfaceTable (Interface, DAG). Set DESTINED_FOR_LOCALHOST route for new AD and delete route for old AD.
	// if RHID changes for default interface - set default routes for AD, HID, 4ID and RHID to go to newRHID.
	// For all RHID changes - delete route for old RHID and add route for new RHID in its place.
	// If NS changes - Do we allow more than one name server here?????
	// How do we send register broadcast message only on a specific interface?
	// Do we register all DAGs with name server or just the default one? May be best to have an API that just gets the DAG instead of components.
	/*
	int rc;
	//NITIN sockaddr_x sdag;
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
	char *router_ad = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *router_hid = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *router_4id = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	char *ns_dag = (char *)malloc(XHCP_MAX_DAG_LENGTH);
	sockaddr_x pseudo_gw_router_dag; // dag for host_register_message (broadcast message), but only the gw router will accept it
	string host_register_message;
	char mydummyAD[MAX_XID_SIZE];
	char myrealAD[MAX_XID_SIZE];
	char myHID[MAX_XID_SIZE]; 
	char my4ID[MAX_XID_SIZE];	
	int changed;

	// make the response message dest DAG (intended destination: gw router who is running the routing process)
	Graph gw = Node() * Node(BHID) * Node(SID_XROUTE);
	gw.fill_sockaddr(&pseudo_gw_router_dag);

   	// read the localhost HID 
   	if ( XreadLocalHostAddr(sockfd, mydummyAD, MAX_XID_SIZE, myHID, MAX_XID_SIZE, my4ID, MAX_XID_SIZE) < 0 ) {
		syslog(LOG_ERR, "unable to retrieve local host address");
		Xclose(sockfd);
	}
	*/

	
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
	while(1) {
		int iface;
		char buf[XIA_MAXBUF];

		if(get_beacon(sockfd, &iface, buf, XIA_MAXBUF)) {
			syslog(LOG_ERR, "ERROR receiving beacon\n");
			continue;
		}
		XHCPBeacon beacon(buf);
		/*
		// validate pkt
		if (strlen(router_ad) <= 0 || strlen(router_hid) <= 0 || strlen(router_4id) <= 0 || strlen(ns_dag) <= 0) {
			syslog(LOG_WARNING, "xhcp_client:main: ERROR invalid beacon packet received");
			continue;
		}
		*/
		if(interfaces[iface].isActive()) {
			printf("Updating routing for interface %d\n", iface);
			if(interfaces[iface].getAD().compare(beacon.getAD())) {
				printf("AD for interface %d changed from %s to %s\n", iface, interfaces[iface].getAD().c_str(), beacon.getAD().c_str());
				interfaces[iface].setAD(beacon.getAD());
			}
			if(interfaces[iface].getRouterHID().compare(beacon.getRouterHID())) {
				printf("Router for interface %d changed from %s to %s\n", iface, interfaces[iface].getRouterHID().c_str(), beacon.getRouterHID().c_str());
				interfaces[iface].setRouterHID(beacon.getRouterHID());
			}
			if(interfaces[iface].getRouter4ID().compare(beacon.getRouter4ID())) {
				printf("Router 4ID for interface %d changed from %s to %s\n", iface, interfaces[iface].getRouter4ID().c_str(), beacon.getRouter4ID().c_str());
				interfaces[iface].setRouter4ID(beacon.getRouter4ID());
			}
			if(interfaces[iface].getNameServerDAG().compare(beacon.getNameServerDAG())) {
				printf("Name server for interface %d changed from %s to %s\n", iface, interfaces[iface].getNameServerDAG().c_str(), beacon.getNameServerDAG().c_str());
				interfaces[iface].setNameServerDAG(beacon.getNameServerDAG());
			}
		} else {
			printf("Creating new entry for interface %d\n", iface);
			interfaces[iface].setID(iface);
			interfaces[iface].setAD(beacon.getAD());
			interfaces[iface].setRouterHID(beacon.getRouterHID());
			interfaces[iface].setRouter4ID(beacon.getRouter4ID());
			interfaces[iface].setNameServerDAG(beacon.getNameServerDAG());
		}
		/*
		
		AD = router_ad;
		gwRHID = router_hid;
		gwR4ID = router_4id;
		nsDAG = ns_dag;
		
		changed = 0;

		// Check if myAD has been changed
		if(myAD.compare(AD) != 0) {
			changed = 1;
			syslog(LOG_INFO, "AD updated");
			// update new AD information
			if(XupdateAD(sockfd, router_ad, router_4id) < 0) {
				syslog(LOG_WARNING, "Error updating my AD to click");
			}
		
			// delete obsolete myAD entry
			xr.delRoute(myAD);
		
			// update AD table (my AD entry)
			if ((rc = xr.setRoute(AD, DESTINED_FOR_LOCALHOST, empty_str, 0xffff)) != 0) {
				syslog(LOG_WARNING, "error setting route %d\n", rc);
			}
				
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
				
			myGWRHID = gwRHID;
		}
		
		// Check if myNS_DAG has been changed
		if(myNS_DAG.compare(nsDAG) != 0) {

			syslog(LOG_INFO, "nameserver changed");
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
			// Message format (delimiter=^)
				// message-type{HostRegister = 2}
				// host-HID
			//
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
			if(changed) {
				sleep(5);
				syslog(LOG_INFO, "xhcp_client: AD or NS changed, resend registration message");
				Xsendto(sockfd, buffer, strlen(buffer), 0, (sockaddr*)&pseudo_gw_router_dag, sizeof(pseudo_gw_router_dag));
			}
		}

		//Register this hostname to the name server
		if (update_ns && beacon_reception_count >= XHCP_CLIENT_NAME_REGISTER_WAIT) {	
			int tmpsockfd = Xsocket(AF_XIA, SOCK_DGRAM, 0);
			if (tmpsockfd < 0) {
				syslog(LOG_ERR, "unable to create a socket");
				exit(-1);
			}
	
			// read the localhost HID 
			if ( XreadLocalHostAddr(tmpsockfd, myrealAD, MAX_XID_SIZE, myHID, MAX_XID_SIZE, my4ID, MAX_XID_SIZE) < 0 ) {
				syslog(LOG_WARNING, "error reading localhost address"); 
				Xclose(tmpsockfd);
				continue;
			}
			Xclose(tmpsockfd);

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

			// Also notify the rendezvous service of this change
			if(XupdateRV(sockfd) < 0) {
				syslog(LOG_ERR, "Unable to update rendezvous server with new locator");
			}

			if(XrendezvousUpdate(myHID, &hdag)) {
				syslog(LOG_ERR, "error updating rendezvous service for %s", myHID);
				beacon_reception_count = 0;
			} else {
				syslog(LOG_INFO, "updated %s as %s at rendezvous", myHID, hg.dag_string().c_str());
			}
		}   
*/
	}	
	return 0;
}

