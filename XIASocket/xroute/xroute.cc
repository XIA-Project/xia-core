#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>

#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include "Xsocket.h"
#include "xroute.hh"


RouteState route_state;

XIARouter xr;


void listRoutes(std::string xidType)
{
	int rc;

	vector<XIARouteEntry> routes;

	printf("\nroutes for AD\n");

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


void timeout_handler(int signum)
{
	
	if (route_state.hello_seq < route_state.hello_lsa_ratio) {
		// send Hello
		sendHello();
		route_state.hello_seq++;
		
	} else if (route_state.hello_seq == route_state.hello_lsa_ratio) {
		// it's time to send LSA
		sendLSA();
	
		// reset hello req
		route_state.hello_seq = 0;
		
	} else {
		printf("error: hello_seq=%d hello_lsa_ratio=%d\n", route_state.hello_seq, route_state.hello_lsa_ratio);
	}
	
	
	// reset the timer
	signal(SIGALRM, timeout_handler);
	alarm(HELLO_INTERVAL);
}


void initRouteState()
{
	
    	// make the dest DAG (broadcast to other routers)
    	route_state.ddag = (char*)malloc(snprintf(NULL, 0, "RE %s %s", BHID, SID_XROUTE) + 1);
    	sprintf(route_state.ddag, "RE %s %s", BHID, SID_XROUTE);	

	// make the src DAG (the one the routing process listens on)
    	route_state.sdag = (char*) malloc(snprintf(NULL, 0, "RE %s %s %s", AD0, RHID0, SID_XROUTE) + 1);
    	sprintf(route_state.sdag, "RE %s %s %s", AD0, RHID0, SID_XROUTE); 
    		
    	// store myAD and myHID
    	route_state.myAD = (char*)malloc(snprintf(NULL, 0, "%s", AD0) + 1);
    	sprintf(route_state.myAD, "%s", AD0);
     	route_state.myHID = (char*)malloc(snprintf(NULL, 0, "%s", RHID0) + 1);
    	sprintf(route_state.myHID, "%s", RHID0);   	
	
	route_state.num_neighbors = 0; // number of neighbor routers
	route_state.lsa_seq = 0;	// LSA sequence number of this router
	route_state.hello_seq = 0;  // hello seq number of this router 	
	route_state.hello_lsa_ratio = ceil(LSA_INTERVAL/HELLO_INTERVAL);
	route_state.calc_dijstra_ticks = 0;

	// set timer for HELLO/LSA
	signal(SIGALRM, timeout_handler);  
	alarm(HELLO_INTERVAL); 	
}


// send Hello message (1-hop broadcast)
int sendHello(){
	// Send my AD and my HID to the directly connected neighbors
	char buffer[1024]; 
	bzero(buffer, 1024);	

	/* Message format (delimiter=^)
		message-type{Hello=0 or LSA=1}
		source-AD
		source-HID
	*/
	string hello;
	hello.append("0^");
	hello.append(route_state.myAD);
	hello.append("^");
	hello.append(route_state.myHID);
	hello.append("^");
	strcpy (buffer, hello.c_str());
printf("\nsend Hello at (%s) msg= %s\n", route_state.myAD, buffer);			
	Xsendto(route_state.sock, buffer, strlen(buffer), 0, route_state.ddag, strlen(route_state.ddag)+1);
	return 1;
}

// send LinkStateAdvertisement message (flooding)
int sendLSA() {
	char buffer[1024]; 
	bzero(buffer, 1024);	
	
	/* Message format (delimiter=^)
		message-type{Hello=0 or LSA=1}
		source-AD
		source-HID
		LSA-seq-num
		num_neighbors
		neighbor1-AD
		neighbor1-HID
		neighbor2-AD
		neighbor2-HID
		...		
	*/
	string lsa;
	char lsa_seq[10], num_neighbors[10];
	
	sprintf(lsa_seq, "%d", route_state.lsa_seq);
	sprintf(num_neighbors, "%d", route_state.num_neighbors);
	
	lsa.append("1^");
	lsa.append(route_state.myAD);
	lsa.append("^");
	lsa.append(route_state.myHID);
	lsa.append("^");
	lsa.append(lsa_seq);
	lsa.append("^");
	lsa.append(num_neighbors);
	lsa.append("^");	
	
	map<std::string, NeighborEntry>::iterator it;
	
  	for ( it=route_state.neighborTable.begin() ; it != route_state.neighborTable.end(); it++ ) {
		lsa.append( it->second.AD );
		lsa.append("^");
		lsa.append( it->second.HID );
		lsa.append("^");
  	}
  	
	strcpy (buffer, lsa.c_str());
	
	// increase the LSA seq
	route_state.lsa_seq++;
	route_state.lsa_seq = route_state.lsa_seq % MAX_SEQNUM;
printf("\nsend LSA at (%s) msg= %s\n", route_state.myAD, buffer);		
	Xsendto(route_state.sock, buffer, strlen(buffer), 0, route_state.ddag, strlen(route_state.ddag)+1);
	return 1;
}



// process an incoming Hello message
int processHello(const char* hello_msg) {
printf("\nReceived Hello at (%s) msg= %s\n", route_state.myAD, hello_msg);
	/* Procedure:
		1. fill in the neighbor table
		2. update my entry in the networkTable
	*/
	
	// 1. fill in the neighbor table
	size_t found, start;
	string msg, neighborAD, neighborHID, myAD;
	
	start = 0;
	msg = hello_msg;
  				
 	// read message-type
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		start = found+1;   // message-type was previously read
  	}
  	 					
	// read neighborAD
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		neighborAD = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
 
 	// read neighborHID
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		neighborHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	} 	
	
	// fill in the table
	map<std::string, NeighborEntry>::iterator it;
	it=route_state.neighborTable.find(neighborAD);
	if(it == route_state.neighborTable.end()) {
		// if no entry yet
		NeighborEntry entry;
		entry.AD = neighborAD;
		entry.HID = neighborHID;
		entry.cost = 1; // for now, same cost
		route_state.neighborTable[neighborAD] = entry;
		
		// increase the neighbor count 
		route_state.num_neighbors++;
	}
	
	
	
	// 2. update my entry in the networkTable
	myAD = route_state.myAD;
	
	map<std::string, NodeStateEntry>::iterator it2;
	it2=route_state.networkTable.find(myAD);
	
	if(it2 != route_state.networkTable.end()) {
  		
  		// For now, delete my entry in networkTable (... we will re-insert the updated entry shortly)
  		route_state.networkTable.erase (it2);	
  	}
  	
	NodeStateEntry entry;
	entry.dest = myAD;
	entry.seq = route_state.lsa_seq;
	entry.num_neighbors = route_state.num_neighbors;	
  
  	map<std::string, NeighborEntry>::iterator it3;
  	for ( it3=route_state.neighborTable.begin() ; it3 != route_state.neighborTable.end(); it3++ ) {

 		// fill my neighbors into my entry in the networkTable
 		entry.neighbor_list.push_back(it3->second.AD);
  	}
 	
	route_state.networkTable[myAD] = entry;  	
	
	
	return 1;
}

// process a LinkStateAdvertisement message 
int processLSA(const char* lsa_msg) {
	printf("\nReceived LSA at (%s) msg= %s\n", route_state.myAD, lsa_msg);
	
	char buffer[1024]; 
	bzero(buffer, 1024);	
	
	/* Procedure:
		0. scan this LSA
		1. filter out the already seen LSA (via LSA-seq for this dest)
		2. update the network table
		3. rebroadcast this LSA
	*/
	
	
	// 0. Read this LSA
	
	size_t found, start;
	string msg, destAD, destHID, lsa_seq, num_neighbors, neighborAD, neighborHID;
	
	start = 0;
	msg = lsa_msg;
  				
 	// read message-type
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		start = found+1;   // message-type was previously read
  	}
  	 					
	// read destAD
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		destAD = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
 
 	// read destHID
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		destHID = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	} 	
	
	// read LSA-seq-num
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		lsa_seq = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	}
  	int lsaSeq = atoi(lsa_seq.c_str());
  	
 	// read num_neighbors
	found=msg.find("^", start);
  	if (found!=string::npos) {
  		num_neighbors = msg.substr(start, found-start);
  		start = found+1;  // forward the search point
  	} 	
  	int numNeighbors = atoi(num_neighbors.c_str());
  	  	
  	
  	// First, filter out the LSA originating from myself
  	string myAD = route_state.myAD;
  	if (myAD.compare(destAD) == 0) {
  		printf("Drop\n");
  		return 1;
  	}
  	
  	
  	// 1. Filter out the already seen LSA
	
	map<std::string, NodeStateEntry>::iterator it;
	it=route_state.networkTable.find(destAD);
	
	if(it != route_state.networkTable.end()) {  	
  		// If this originating AD has been known (i.e., already in the networkTable)
  		
  	  	if (lsaSeq <= it->second.seq  &&  it->second.seq - lsaSeq < 10000) {
  	  		// If this LSA already seen, ignore this LSA; do nothing
  			return 1;
  		}
  		
  		// For now, delete this dest AD entry in networkTable (... we will re-insert the updated entry shortly)
  		route_state.networkTable.erase (it);	
  	}
  	
  	
	
	// 2. Update the network table
	NodeStateEntry entry;
	entry.dest = destAD;
	entry.seq = lsaSeq;
	entry.num_neighbors = numNeighbors;	
  	
  	int i;
 	for (i = 0; i < numNeighbors; i++) {
 	
 		// read neighborAD
		found=msg.find("^", start);
  		if (found!=string::npos) {
  			neighborAD = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		}
 
 		// read neighborHID
		found=msg.find("^", start);
  		if (found!=string::npos) {
  			neighborHID = msg.substr(start, found-start);
  			start = found+1;  // forward the search point
  		} 	
 	
 		// fill the neighbors into the corresponding networkTable entry
 		entry.neighbor_list.push_back(neighborAD);
 	
 	}
 	
	route_state.networkTable[destAD] = entry;  
	
	
	printf("LSA process: dest=%s, seq=%d, num_neighbors=%d, neighbors=%s \n", (route_state.networkTable[destAD].dest).c_str(), route_state.networkTable[destAD].seq, route_state.networkTable[destAD].num_neighbors, (route_state.networkTable[destAD].neighbor_list.back()).c_str());
	
	
	route_state.calc_dijstra_ticks++;
	
	if (route_state.calc_dijstra_ticks == CALC_DIJKSTRA_INTERVAL) {
		// Calculate Shortest Path algorithm
		calcShortestPath();
		route_state.calc_dijstra_ticks = 0;
		// update Routing table (click routing table as well)	
	
	}	
	
	
	// 5. rebroadcast this LSA	
	strcpy (buffer, lsa_msg);
	Xsendto(route_state.sock, buffer, strlen(buffer), 0, route_state.ddag, strlen(route_state.ddag)+1);

	return 1;
}


void calcShortestPath() {

	// first, clear the current routing table
	route_state.ADrouteTable.clear();

	// check the current networkTable
	int numNode = (int)(route_state.networkTable.size());
	map<std::string, NodeStateEntry> table; 
	table = route_state.networkTable;
	
  	map<std::string, NodeStateEntry>::iterator it1;
  	for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {
 		// initialize the checking variable
 		it1->second.checked = false;
 		it1->second.cost = 10000000;
  	}
	
	// compute shortest path
	// initialization
	string myAD, tempAD;
	myAD = route_state.myAD;
	route_state.networkTable[myAD].checked = true;
	route_state.networkTable[myAD].cost = 0;
	table.erase(myAD);
	
	vector<std::string>::iterator it2;
	for ( it2=route_state.networkTable[myAD].neighbor_list.begin() ; it2 < route_state.networkTable[myAD].neighbor_list.end(); it2++ ) {
	
		tempAD = (*it2).c_str();
		route_state.networkTable[tempAD].cost = 1;
		route_state.networkTable[tempAD].prevNode = myAD;
	}
	
	// loop
	while (!table.empty()) {
		int minCost = 10000000;
		string selectedAD, tmpAD;
		
		for ( it1=table.begin() ; it1 != table.end(); it1++ ) {
			
			tmpAD = it1->second.dest;
			if (route_state.networkTable[tmpAD].cost < minCost) {
				minCost = route_state.networkTable[tmpAD].cost;
				selectedAD = tmpAD;
			}
  		}
  		
  		table.erase(selectedAD);
  		route_state.networkTable[selectedAD].checked = true;
 
 		for ( it2=route_state.networkTable[selectedAD].neighbor_list.begin() ; it2 < route_state.networkTable[selectedAD].neighbor_list.end(); it2++ ) {
	
			tempAD = (*it2).c_str();
			if (route_state.networkTable[tempAD].checked != true) {
				
				if (route_state.networkTable[tempAD].cost > route_state.networkTable[selectedAD].cost + 1) {
					
					route_state.networkTable[tempAD].cost = route_state.networkTable[selectedAD].cost + 1;
					route_state.networkTable[tempAD].prevNode = selectedAD;
				}
				
			}
			
		} 		
	
  		
	}
	
	string tempAD1, tempAD2;
	
	// set up the nexthop
  	for ( it1=route_state.networkTable.begin() ; it1 != route_state.networkTable.end(); it1++ ) {
  	
  		tempAD1 = it1->second.dest;
  		if ( myAD.compare(tempAD1) != 0 ) {
  			
  			tempAD2 = tempAD1;
  			while (route_state.networkTable[tempAD2].prevNode.compare(myAD)!=0) {
  				tempAD2 = route_state.networkTable[tempAD2].prevNode;
  			}
  		
  			route_state.ADrouteTable[tempAD1].dest = tempAD1;
  			route_state.ADrouteTable[tempAD1].nextHop = route_state.neighborTable[tempAD2].HID;
  		}
  		
  	}	
	
	printRoutingTable();		

}


void printRoutingTable() {

	printf("AD Routing table at %s\n", route_state.myAD);
  	map<std::string, RouteEntry>::iterator it1;
  	for ( it1=route_state.ADrouteTable.begin() ; it1 != route_state.ADrouteTable.end(); it1++ ) {
  		printf("Dest=%s, NextHop=%s, Port=%d, Flags=%lu \n", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port), (it1->second.flags) );

  	}

}



int main()
{
	int rc, x, selectRetVal, n;
    	size_t dlen, found, start;
    	char recv_message[1024];
    	char buffer[2048], theirDAG[1024];   
    	fd_set socks;
    	struct timeval timeoutval;	
	vector<string> routers;
	
	
    	// connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click! (%d)\n", rc);
		return -1;
	}
	
    	// initialize the route states (e.g., set HELLO/LSA timer, etc)
    	initRouteState();

    	// open socket for route process
    	route_state.sock=Xsocket(XSOCK_DGRAM);
    	if (route_state.sock < 0) 
    		error("Opening socket");
   
    	// bind to the src DAG
    	Xbind(route_state.sock, route_state.sdag);
    	
	
	while (1) {
		FD_ZERO(&socks);
		FD_SET(route_state.sock, &socks);
		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 20000; // every 0.02 sec, check if any received packets
		
		selectRetVal = select(route_state.sock+1, &socks, NULL, NULL, &timeoutval); 
		
		if (selectRetVal > 0) {
			// receiving a Hello or LSA packet
			n = Xrecvfrom(route_state.sock, recv_message, 1024, 0, theirDAG, &dlen);
			if (n < 0) 
	    			error("recvfrom");
			
			string msg = recv_message;
			start = 0;
			found=msg.find("^");
  			if (found!=string::npos) {
  				string msg_type = msg.substr(start, found-start);
  				int type = atoi(msg_type.c_str());
  				
  				if (type == 0) {
  					// process this incoming Hello message
  					processHello(msg.c_str());
  				
  				} else if (type == 1) {
  					// process this incoming Hello message
  					processLSA(msg.c_str());  				
  				}
  				
  				
  			}
			
				
		} 
    	
    	}


	return 0;
}
