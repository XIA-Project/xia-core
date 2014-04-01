#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <libgen.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include <map>
#include <time.h>
#include <math.h>

#include <sys/types.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include "Xsocket.h"
#include "xcontrold.hh"
#include "dagaddr.hpp"
#include "minIni.h"

#define DEFAULT_NAME "controller0"
#define APPNAME "xcontrold"
#define EXPIRE_TIME 600

char *hostname = NULL;
char *ident = NULL;

int ctrl_sock;
RouteState route_state;
XIARouter xr;
map<string,time_t> timeStamp;

void timeout_handler(int signum)
{
	UNUSED(signum);

	if (route_state.hello_seq < route_state.hello_lsa_ratio) {
		// send Hello
		route_state.send_hello = true;
		//sendHello();
		route_state.hello_seq++;
	} else if (route_state.hello_seq >= route_state.hello_lsa_ratio) {
		// it's time to send LSA
		route_state.send_lsa = true;
		//sendInterdomainLSA();
		// reset hello req
		route_state.hello_seq = 0;
	} else {
		syslog(LOG_ERR, "hello_seq=%d hello_lsa_ratio=%d", route_state.hello_seq, route_state.hello_lsa_ratio);
	}
	 //TODO: only send discovery message when necessary
    if (route_state.sid_discovery_seq < route_state.hello_sid_discovery_ratio) {
        //wait
        route_state.sid_discovery_seq++;
    } else if (route_state.sid_discovery_seq >= route_state.hello_sid_discovery_ratio ) {
        //send sid discovery message
        route_state.send_sid_discovery = true;
        route_state.sid_discovery_seq = 0;
    }
    if (route_state.sid_decision_seq < route_state.hello_sid_decision_ratio) {
        //wait
        route_state.sid_decision_seq++;
    } else if (route_state.sid_decision_seq >= route_state.hello_sid_decision_ratio ) {
        //send sid decision message
        route_state.send_sid_decision = true;
        route_state.sid_decision_seq = 0;
    }
	// reset the timer
	signal(SIGALRM, timeout_handler);
	ualarm((int)ceil(HELLO_INTERVAL*1000000),0);
}

// send Hello message (1-hop broadcast) with my AD and my HID to the directly connected neighbors
int sendHello()
{
	ControlMessage msg1(CTL_HELLO, route_state.myAD, route_state.myHID);
	int rc1 = msg1.send(route_state.sock, &route_state.ddag);

	// Advertize controller service
	ControlMessage msg2(CTL_HELLO, route_state.myAD, route_state.myHID);
	msg2.append(SID_XCONTROL);
	int rc2 = msg2.send(route_state.sock, &route_state.ddag);

	return (rc1 < rc2)? rc1 : rc2;
}

// send LinkStateAdvertisement message to neighboring ADs (flooding)
/* Message format (delimiter=^)
    message-type{LSA=1}
    source-AD
    source-HID
    router-type{XIA=0 or XIA-IPv4-Dual=1}
    LSA-seq-num
    num_neighbors
    neighbor1-AD
    neighbor1-HID
    neighbor2-AD
    neighbor2-HID
    ...
*/
int sendInterdomainLSA()
{
	int rc = 1;
    ControlMessage msg(CTL_XBGP, route_state.myAD, route_state.myAD);

    msg.append(route_state.dual_router);
    msg.append(route_state.lsa_seq);
    msg.append(route_state.ADNeighborTable.size());

    std::map<std::string, NeighborEntry>::iterator it;
    for (it = route_state.ADNeighborTable.begin(); it != route_state.ADNeighborTable.end(); it++)
    {
        msg.append(it->second.AD);
        msg.append(it->second.HID);
        msg.append(it->second.port);
        msg.append(it->second.cost);
	}

    for (it = route_state.ADNeighborTable.begin(); it != route_state.ADNeighborTable.end(); it++)
    {
		sockaddr_x ddag;
		Graph g = Node() * Node(it->second.AD) * Node(SID_XCONTROL);
		g.fill_sockaddr(&ddag);

		//syslog(LOG_INFO, "send inter-AD LSA[%d] to %s", route_state.lsa_seq, it->second.AD.c_str());
		//syslog(LOG_INFO, "msg: %s", msg.c_str());
		int temprc = msg.send(route_state.sock, &ddag);
		if (temprc < 0) {
			syslog(LOG_ERR, "error sending inter-AD LSA to %s", it->second.AD.c_str());
		}
		rc = (temprc < rc)? temprc : rc;
  	}

	route_state.lsa_seq = (route_state.lsa_seq + 1) % MAX_SEQNUM;
	return rc;
}

int processInterdomainLSA(ControlMessage msg)
{
	// 0. Read this LSA
	int32_t isDualRouter, numNeighbors, lastSeq;
	string srcAD, srcHID;

	msg.read(srcAD);
	msg.read(srcHID);
	msg.read(isDualRouter);


	// See if this LSA comes from AD with dualRouter
	if (isDualRouter == 1)
		route_state.dual_router_AD = srcAD;

	// First, filter out the LSA originating from myself
	if (srcAD == route_state.myAD)
		return 1;

	msg.read(lastSeq);

	// 1. Filter out the already seen LSA
	if (route_state.ADLastSeqTable.find(srcAD) != route_state.ADLastSeqTable.end()) {
		int32_t old = route_state.ADLastSeqTable[srcAD];
		if (lastSeq <= old && (old - lastSeq) < SEQNUM_WINDOW) {
			// drop the old LSA update.
			return 1;
		}
	}

	//syslog(LOG_INFO, "inter-AD LSA[%d] from %s", lastSeq, srcAD.c_str());

	route_state.ADLastSeqTable[srcAD] = lastSeq;
	
	msg.read(numNeighbors);

	// 2. Update the network table
	NodeStateEntry entry;
	entry.ad = srcAD;
	entry.hid = srcAD;
	entry.num_neighbors = numNeighbors;

	for (int i = 0; i < numNeighbors; i++)
	{
		NeighborEntry neighbor;
		msg.read(neighbor.AD);
		msg.read(neighbor.HID);
		msg.read(neighbor.port);
		msg.read(neighbor.cost);

		//syslog(LOG_INFO, "neighbor[%d] = %s", i, neighbor.AD.c_str());

		entry.neighbor_list.push_back(neighbor);
	}

	route_state.ADNetworkTable[srcAD] = entry;

	// Rebroadcast this LSA
    int rc = 1;
	std::map<std::string, NeighborEntry>::iterator it;
    for (it = route_state.ADNeighborTable.begin(); it != route_state.ADNeighborTable.end(); it++) {
		sockaddr_x ddag;
		Graph g = Node() * Node(it->second.AD) * Node(SID_XCONTROL);
		g.fill_sockaddr(&ddag);

		int temprc = msg.send(route_state.sock, &ddag);
		rc = (temprc < rc)? temprc : rc;
	}
	return rc;
}

int sendRoutingTable(std::string destHID, std::map<std::string, RouteEntry> routingTable)
{
	if (destHID == route_state.myHID) {
		// If destHID is self, process immediately
		return processRoutingTable(routingTable);
	} else {
		// If destHID is not SID, send to relevant router
		ControlMessage msg(CTL_ROUTING_TABLE, route_state.myAD, route_state.myHID);

		msg.append(route_state.myAD);
		msg.append(destHID);

		msg.append(route_state.ctl_seq);

		msg.append((int)routingTable.size());

		map<string, RouteEntry>::iterator it;
		for (it = routingTable.begin(); it != routingTable.end(); it++)
		{
			msg.append(it->second.dest);
			msg.append(it->second.nextHop);
			msg.append(it->second.port);
			msg.append(it->second.flags);
		}

		route_state.ctl_seq = (route_state.ctl_seq + 1) % MAX_SEQNUM;

		return msg.send(route_state.sock, &route_state.ddag);
	}
}

int sendKeepAliveToServiceController()
{
    int rc = 1;
    int type = 0;

    std::map<std::string, ServiceState>::iterator it;
    for (it = route_state.LocalSidList.begin(); it != route_state.LocalSidList.end(); ++it)
    {
        ControlMessage msg(CTL_SID_MANAGE_KA, route_state.myAD, route_state.myHID);
        msg.append(it->first);
        msg.append(it->second.capacity);
        // TODO:append some more states

        if (it->second.isController){
            msg.read(type); // remove it to match the correct format for the process function
            processServiceKeepAlive(msg); // process locally
        }
        else
        {
            sockaddr_x ddag;
            Graph g(it->second.controllerAddr);
            g.fill_sockaddr(&ddag);
            int temprc = msg.send(route_state.sock, &ddag);
            if (temprc < 0) {
                syslog(LOG_ERR, "error sending SID keep alive to %s", it->second.controllerAddr.c_str());
            }
            rc = (temprc < rc)? temprc : rc;
            //syslog(LOG_DEBUG, "sent SID %s keep alive to %s", it->first.c_str(), it->second.controllerAddr.c_str());
        }
    }

    return rc;
}

int processServiceKeepAlive(ControlMessage msg)
{
    int rc = 1;
    string srcAD, srcHID;

    std::string sid;
    int capacity;

    msg.read(srcAD);
    msg.read(srcHID);
    string srcAddr = "RE " + srcAD + " " + srcHID;
    msg.read(sid);
    msg.read(capacity);

    if (route_state.LocalServiceControllers.find(sid) != route_state.LocalServiceControllers.end())
    { // if controller is here
        route_state.LocalServiceControllers[sid].instances[srcAddr].capacity = capacity; //update attributes
        // TODO: reply to that instance?
        //syslog(LOG_DEBUG, "got SID %s keep alive from %s", sid.c_str(), srcAddr.c_str());
    }
    else
    {
        syslog(LOG_ERR, "got SID %s keep alive to its service controller from %s, but I am not!", sid.c_str(), srcAddr.c_str());
        rc = -1;
    }

    return rc;
}

int sendSidDiscovery()
{
    int rc = 1;
    ControlMessage msg(CTL_SID_DISCOVERY, route_state.myAD, route_state.myAD);

    //broadcast local services
    if (route_state.LocalSidList.empty() && route_state.SIDADsTable.empty())
    {
        // Nothing to send
        // syslog(LOG_INFO, "%s LocalSidList is empty, nothing to send", route_state.myAD);
        return rc;
    }
    else // prepare the packet TODO: only send updated entries TODO: but don't forget to renew TTL
    {
        sendKeepAliveToServiceController(); // temporally put it here
        // DOTO: the format of this packet could be reduced much
        // local info
        msg.append(route_state.LocalSidList.size());
        std::map<std::string, ServiceState>::iterator it;
        for (it = route_state.LocalSidList.begin(); it != route_state.LocalSidList.end(); ++it)
        {
            msg.append(route_state.myAD); //unified form : AD SID pairs TODO: make it compact
            msg.append(it->first);
            // TODO: append more attributes/parameters
            msg.append(it->second.capacity);
            msg.append(it->second.capacity_factor);
            msg.append(it->second.link_factor);
            msg.append(it->second.priority);
            msg.append(it->second.controllerAddr);
            msg.append(it->second.archType);
        }

        // rebroadcast services learnt from others
        msg.append(route_state.SIDADsTable.size());
        std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
        for (it_sid = route_state.SIDADsTable.begin(); it_sid != route_state.SIDADsTable.end(); ++it_sid)
        {
            msg.append(it_sid->second.size());
            std::map<std::string, ServiceState>::iterator it_ad;
            for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
            {
                msg.append(it_ad->first);
                msg.append(it_sid->first);
                // TODO: put information from service_state inside
                msg.append(it_ad->second.capacity);
                msg.append(it_ad->second.capacity_factor);
                msg.append(it_ad->second.link_factor);
                msg.append(it_ad->second.priority);
                msg.append(it_ad->second.controllerAddr);
                msg.append(it_ad->second.archType);
            }
        }
    }

    // send it to neighbor ADs
    // TODO: reuse code: broadcastToADNeighbors(msg)?
    std::map<std::string, NeighborEntry>::iterator it;

    for (it = route_state.ADNeighborTable.begin(); it != route_state.ADNeighborTable.end(); ++it)
    {
        sockaddr_x ddag;
        Graph g = Node() * Node(it->second.AD) * Node(SID_XCONTROL);
        g.fill_sockaddr(&ddag);

        //syslog(LOG_INFO, "send inter-AD LSA[%d] to %s", route_state.lsa_seq, it->second.AD.c_str());
        //syslog(LOG_INFO, "msg: %s", msg.c_str());
        int temprc = msg.send(route_state.sock, &ddag);
        if (temprc < 0) {
            syslog(LOG_ERR, "error sending inter-AD SID discovery to %s", it->second.AD.c_str());
        }
        rc = (temprc < rc)? temprc : rc;
    }

    route_state.sid_discovery_seq = (route_state.sid_discovery_seq + 1) % MAX_SEQNUM;
    return rc;
}

int processSidDiscovery(ControlMessage msg)
{
    //TODO: add version(time-stamp?) for each entry
    int rc = 1;
    string srcAD, srcHID;

    string AD, SID;
    int records = 0;
    int capacity, capacity_factor, link_factor, priority;
    int archType;
    string controllerAddr;

    msg.read(srcAD);
    msg.read(srcHID);
    //syslog(LOG_INFO, "Get SID discovery msg from %s", srcAD.c_str());

    // process the entries: AD-SID pairs
    msg.read(records);//number of entries
    //syslog(LOG_INFO, "Get %d origin SIDs", records);
    for (int i = 0; i < records; ++i)
    {
        ServiceState service_state;
        msg.read(AD);
        msg.read(SID);
        //TODO: read the parameters from msg, put them into service_state
        msg.read(capacity);
        msg.read(capacity_factor);
        msg.read(link_factor);
        msg.read(priority);
        msg.read(controllerAddr);
        msg.read(archType);
        service_state.capacity = capacity;
        service_state.capacity_factor = capacity_factor;
        service_state.link_factor = link_factor;
        service_state.priority = priority;
        service_state.controllerAddr = controllerAddr;
        service_state.archType = archType;
        updateSidAdsTable(AD, SID, service_state);
    }

    //process the re-broadcast entries SID:[ADs]
    msg.read(records);//number of re-broadcast sids
    //syslog(LOG_INFO, "Get %d re-broadcast SIDs", records);
    for (int i = 0; i < records; ++i)
    {
        int ad_records = 0;
        msg.read(ad_records);
        for (int j = 0; j < ad_records; ++j)
        {
            ServiceState service_state;
            msg.read(AD);
            msg.read(SID);
            // TODO: read the parameters from msg
            msg.read(capacity);
            msg.read(capacity_factor);
            msg.read(link_factor);
            msg.read(priority);
            msg.read(controllerAddr);
            msg.read(archType);
            service_state.controllerAddr = controllerAddr;
            service_state.archType = archType;
            service_state.capacity = capacity;
            service_state.capacity_factor = capacity_factor;
            service_state.link_factor = link_factor;
            service_state.priority = priority;
            updateSidAdsTable(AD, SID, service_state);
        }
    }

    //syslog(LOG_INFO, "SID-ADs %lu", route_state.SIDADsTable.size());
    // rc = processSidDecision(); // use a timeout callback instead?
    // TODO: read the config file frequently, emulate service failure
    // TODO: how/when to revoke/remove a entry
    // TODO: TTL for AD-SID pair for fast failure discovery/recovery
    return rc;
}

int processSidDecision(void)
{
    // make decision based on principles like highest priority first, load balancing, nearest...
    // Using function: (capacity^factor/link^facor)*priority for weight
    int rc = 1;

    //local balance decision: decide which percentage of traffic each AD should has
    std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
    for (it_sid = route_state.SIDADsTable.begin(); it_sid != route_state.SIDADsTable.end(); ++it_sid)
    {
        // calculate the percentage for each AD for this SID
        double total_weight = 0;

        std::map<std::string, ServiceState>::iterator it_ad;
        // find the total weight
        for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
        {
            int latency = 1000; // default, 1ms
            if (route_state.ADPathStates.find(it_ad->first) != route_state.ADPathStates.end()){
                latency = route_state.ADPathStates[it_ad->first].delay;
            }
            it_ad->second.weight = pow(it_ad->second.capacity, it_ad->second.capacity_factor)/pow(latency, it_ad->second.link_factor)*it_ad->second.priority;
            total_weight += it_ad->second.weight;
            //syslog(LOG_INFO, "%s @%s :cap=%d, f=%d, late=%d, f=%d, prio=%d, weight is %f",it_sid->first.c_str(), it_ad->first.c_str(), it_ad->second.capacity, it_ad->second.capacity_factor, latency, it_ad->second.link_factor, it_ad->second.priority, it_ad->second.weight );
        }
            //syslog(LOG_INFO, "total_weight is %f", total_weight );
        // make local decision. map weights to 0..100
        for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
        {
            it_ad->second.valid = true;
            it_ad->second.percentage = (int) 100.0*it_ad->second.weight/total_weight;
            //syslog(LOG_INFO, "percentage is %d", it_ad->second.percentage );
        }
    }
    // for debug
    // dumpSidAdsTable();

    // update every router
    sendSidRoutingDecision();
    return rc;
}

int sendSidRoutingDecision(void)
{
    // TODO: when to call this function timeout? new incoming sid discovery?
    // TODO: if not reusing AD routing table, this function should calculate routing table
    // for each router
    // Now we just send the identical decision to every router. The routers will reuse their
    // own routing table to interpret the decision
    int rc = 1;

    // remap the SIDADsTable
    std::map<std::string, std::map<std::string, ServiceState> > ADSIDsTable;

    std::map<std::string, std::map<std::string, ServiceState> >::iterator it_sid;
    for (it_sid = route_state.SIDADsTable.begin(); it_sid != route_state.SIDADsTable.end(); ++it_sid)
    {
        std::map<std::string, ServiceState>::iterator it_ad;
        for (it_ad = it_sid->second.begin(); it_ad != it_sid->second.end(); ++it_ad)
        {
            if (it_ad->second.valid) // only send choices we want to use
            {
                if (ADSIDsTable.count(it_ad->first) > 0) // has key AD
                {
                    ADSIDsTable[it_ad->first][it_sid->first] = it_ad->second;
                }
                else // create a new map for new AD
                {
                    std::map<std::string, ServiceState> new_map;
                    new_map[it_sid->first] = it_ad->second;
                    ADSIDsTable[it_ad->first] = new_map;
                }
            }
        }
    }

    // for each router:
    std::map<std::string, NodeStateEntry>::iterator it_router;
    for (it_router = route_state.networkTable.begin(); it_router != route_state.networkTable.end(); ++it_router)
    {
        if ((it_router->second.ad != route_state.myAD) || (it_router->second.hid == ""))
        {
            // Don't calculate routes for external ADs
            continue;
        }
        else if (it_router->second.hid.find(string("SID")) != string::npos)
        {
            // Don't calculate routes for SIDs
            continue;
        }
        rc = sendSidRoutingTable(it_router->second.hid, ADSIDsTable);
    }
    return rc;
}

int sendSidRoutingTable(std::string destHID, std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable)
{
    // send routing table to router:destHID. ADSIDsTable is a remapping of SIDADsTable for easier use
    if (destHID == route_state.myHID)
    {
        // If destHID is self, process immediately
        //syslog(LOG_INFO, "set local sid routes");
        return processSidRoutingTable(ADSIDsTable);
    }
    else
    {
        // If destHID is not SID, send to relevant router
        ControlMessage msg(CTL_SID_ROUTING_TABLE, route_state.myAD, route_state.myHID);

        // for checking and resend
        msg.append(route_state.myAD);
        msg.append(destHID);

        // TODO: add ctl_seq to guarantee consistency
        //msg.append(route_state.ctl_seq);

        //Format: #2 AD1:[ #2 SID1(percentage 30),SID2(percentage 100)], AD2[ #1 SID1(percentage 70)]

        msg.append(ADSIDsTable.size());
        std::map<std::string, std::map<std::string, ServiceState> >::iterator it_ad;
        for (it_ad = ADSIDsTable.begin(); it_ad != ADSIDsTable.end(); ++it_ad)
        {
            msg.append(it_ad->second.size());
            msg.append(it_ad->first);
            std::map<std::string, ServiceState>::iterator it_sid;
            for (it_sid = it_ad->second.begin(); it_sid != it_ad->second.end(); ++it_sid)
            {
                msg.append(it_sid->first);
                // TODO: put information from service_state inside
                msg.append(it_sid->second.percentage);
            }
        }
        // TODO: update ctl seq
        //route_state.ctl_seq = (route_state.ctl_seq + 1) % MAX_SEQNUM;

        return msg.send(route_state.sock, &route_state.ddag);
    }
}

int processSidRoutingTable(std::map<std::string, std::map<std::string, ServiceState> > &ADSIDsTable)
{
    int rc = 1;

    std::vector<XIARouteEntry> xrt;
    xr.getRoutes("AD", xrt);

    //change vector to map AD:RouteEntry for faster lookup
    std::map<std::string, XIARouteEntry> ADlookup;
    vector<XIARouteEntry>::iterator ir;
    for (ir = xrt.begin(); ir < xrt.end(); ++ir) {
        ADlookup[ir->xid] = *ir;
    }

    std::map<std::string, std::map<std::string, ServiceState> >::iterator it_ad;
    for (it_ad = ADSIDsTable.begin(); it_ad != ADSIDsTable.end(); ++it_ad)
    {
        XIARouteEntry entry = ADlookup[it_ad->first];
        //syslog(LOG_INFO, "AD entry: %s, %d, %s, %lu", entry.xid.c_str(), entry.port, entry.nextHop.c_str(), entry.flags);
        std::map<std::string, ServiceState>::iterator it_sid;
        for (it_sid = it_ad->second.begin(); it_sid != it_ad->second.end(); ++it_sid)
        {
            // TODO: how to use flags to load balance? or add new fields into routing table for this propose
            // syslog(LOG_INFO, "add route %s, %hu, %s, %lu", it_sid->first.c_str(), entry.port, entry.nextHop.c_str(), entry.flags);
            if (entry.xid == route_state.myAD)
            {
                xr.seletiveSetRoute(it_sid->first, -2, entry.nextHop, entry.flags, it_sid->second.percentage, it_ad->first); //local AD, FIXME: why is port unsigned short? port could be negative numbers!
            }
            else
            {
                xr.seletiveSetRoute(it_sid->first, entry.port, entry.nextHop, entry.flags, it_sid->second.percentage, it_ad->first);
            }

        }
    }

    return rc;
}

int updateSidAdsTable(std::string AD, std::string SID, ServiceState service_state)
{
    int rc = 1;
    //TODO: only record ones with newer version
    //TODO: only update public information
    if ( route_state.SIDADsTable.count(SID) > 0 )
    {
        if (route_state.SIDADsTable[SID].count(AD) > 0 )
        {
            //TODO: update if version is newer
            route_state.SIDADsTable[SID][AD].capacity = service_state.capacity;
            route_state.SIDADsTable[SID][AD].capacity_factor = service_state.capacity_factor;
            route_state.SIDADsTable[SID][AD].link_factor = service_state.link_factor;
            route_state.SIDADsTable[SID][AD].priority = service_state.priority;

            //TODO: update other parameters
        }
        else
        {
            // insert new entry
            route_state.SIDADsTable[SID][AD] = service_state;
        }
    }
    else // no sid record
    {
        std::map<std::string, ServiceState> new_map;
        new_map[AD] = service_state;
        route_state.SIDADsTable[SID] = new_map;
    }
    // TODO: make return value meaningful or make this function return void
    //syslog(LOG_INFO, "update SID:%s, capacity %d, f1 %d, f2 %d, priority %d, from %s", SID.c_str(), service_state.capacity, service_state.capacity_factor, service_state.link_factor, service_state.priority, AD.c_str());
    return rc;
}

int processMsg(std::string msg)
{
    int type, rc = 0;
    ControlMessage m(msg);

    m.read(type);

    switch (type)
    {
        case CTL_HOST_REGISTER:
//            rc = processHostRegister(m);
            break;
        case CTL_HELLO:
            rc = processHello(m);
            break;
        case CTL_LSA:
            rc = processLSA(m);
            break;
        case CTL_ROUTING_TABLE:
            // rc = processRoutingTable(m);
            break;
        case CTL_XBGP:
            rc = processInterdomainLSA(m);
            break;
        case CTL_SID_DISCOVERY:
            rc = processSidDiscovery(m);
            break;
        case CTL_SID_ROUTING_TABLE:
            //the table msg reflects back, ingore
            break;
        case CTL_SID_MANAGE_KA:
            rc = processServiceKeepAlive(m);
            break;
        default:
            perror("unknown routing message");
            break;
    }

    return rc;
}

int interfaceNumber(std::string xidType, std::string xid)
{
	int rc;
	vector<XIARouteEntry> routes;
	if ((rc = xr.getRoutes(xidType, routes)) > 0) {
		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			if ((r.xid).compare(xid) == 0) {
				return (int)(r.port);
			}
		}
	}
	return -1;
}

int processHello(ControlMessage msg)
{
	// Update neighbor table
    NeighborEntry neighbor;
    msg.read(neighbor.AD);
    msg.read(neighbor.HID);
    neighbor.port = interfaceNumber("HID", neighbor.HID);
    neighbor.cost = 1; // for now, same cost

    // Index by HID if neighbor in same domain or by AD otherwise
    bool internal = (neighbor.AD == route_state.myAD);
    route_state.neighborTable[internal ? neighbor.HID : neighbor.AD] = neighbor;
    route_state.num_neighbors = route_state.neighborTable.size();

    // Update network table
    std::string myHID = route_state.myHID;

	NodeStateEntry entry;
	entry.hid = myHID;
	entry.num_neighbors = route_state.num_neighbors;

    // Add neighbors to network table entry
    std::map<std::string, NeighborEntry>::iterator it;
    for (it = route_state.neighborTable.begin(); it != route_state.neighborTable.end(); it++)
 		entry.neighbor_list.push_back(it->second);

	route_state.networkTable[myHID] = entry;

	return 1;
}

int processRoutingTable(std::map<std::string, RouteEntry> routingTable)
{
	int rc;
	map<string, RouteEntry>::iterator it;
	for (it = routingTable.begin(); it != routingTable.end(); it++)
	{
		// TODO check for all published SIDs
		// TODO do this for xrouted as well
		// Ignore SIDs that we publish
		if (it->second.dest == SID_XCONTROL) {
			continue;
		}
		if ((rc = xr.setRoute(it->second.dest, it->second.port, it->second.nextHop, it->second.flags)) != 0)
			syslog(LOG_ERR, "error setting route %d", rc);

        timeStamp[it->second.dest] = time(NULL);
	}

	return 1;
}

/* Procedure:
   0. scan this LSA (mark AD with a DualRouter if there)
   1. filter out the already seen LSA (via LSA-seq for this dest)
   2. update the network table
   3. rebroadcast this LSA
*/
int processLSA(ControlMessage msg)
{
	// 0. Read this LSA
	int32_t isDualRouter, numNeighbors, lastSeq;
	string srcAD, srcHID;

	msg.read(srcAD);
	msg.read(srcHID);
	msg.read(isDualRouter);

	// See if this LSA comes from AD with dualRouter
	if (isDualRouter == 1)
		route_state.dual_router_AD = srcAD;

	// First, filter out the LSA originating from myself
	if (srcHID == route_state.myHID)
		return 1;

	msg.read(lastSeq);

	// 1. Filter out the already seen LSA
	if (route_state.lastSeqTable.find(srcHID) != route_state.lastSeqTable.end()) {
		int32_t old = route_state.lastSeqTable[srcHID];
		if (lastSeq <= old && (old - lastSeq) < SEQNUM_WINDOW) {
			// drop the old LSA update.
			return 1;
		}
	}

	route_state.lastSeqTable[srcHID] = lastSeq;
	
	msg.read(numNeighbors);

	// 2. Update the network table
	NodeStateEntry entry;
	entry.ad = srcAD;
	entry.hid = srcHID;
	entry.num_neighbors = numNeighbors;

	for (int i = 0; i < numNeighbors; i++)
	{
		NeighborEntry neighbor;
		msg.read(neighbor.AD);
		msg.read(neighbor.HID);
		msg.read(neighbor.port);
		msg.read(neighbor.cost);

		entry.neighbor_list.push_back(neighbor);
	}

	route_state.networkTable[srcHID] = entry;
	route_state.calc_dijstra_ticks++;

	if (route_state.calc_dijstra_ticks >= CALC_DIJKSTRA_INTERVAL)
	{
		syslog(LOG_DEBUG, "Calcuating shortest paths\n");

		// Calculate next hop for ADs
		std::map<std::string, RouteEntry> ADRoutingTable;
		populateRoutingTable(route_state.myAD, route_state.ADNetworkTable, ADRoutingTable);
		//printADNetworkTable();
		//printRoutingTable(route_state.myAD, ADRoutingTable);

		// Calculate next hop for routers
		std::map<std::string, NodeStateEntry>::iterator it1;
		for (it1 = route_state.networkTable.begin(); it1 != route_state.networkTable.end(); it1++)
		{
			if ((it1->second.ad != route_state.myAD) || (it1->second.hid == "")) {
				// Don't calculate routes for external ADs
				continue;
			} else if (it1->second.hid.find(string("SID")) != string::npos) {
				// Don't calculate routes for SIDs
				continue;
			}
			std::map<std::string, RouteEntry> routingTable;
			populateRoutingTable(it1->second.hid, route_state.networkTable, routingTable);
			extractNeighborADs(routingTable);
			populateNeighboringADBorderRouterEntries(it1->second.hid, routingTable);
			populateADEntries(routingTable, ADRoutingTable);
			//printRoutingTable(it1->second.hid, routingTable);

			sendRoutingTable(it1->second.hid, routingTable);
		}

		route_state.calc_dijstra_ticks = 0;
	}

	return 1;
}

int extractNeighborADs(map<string, RouteEntry> routingTable)
{
	map<string, RouteEntry>::iterator it;
	for (it = routingTable.begin(); it != routingTable.end(); it++)
	{
		if (it->second.dest.find(string("AD:")) == 0) {
			// If AD, add to AD neighbor table
			// Update neighbor table
			NeighborEntry neighbor;
			neighbor.AD = it->second.dest;
			neighbor.HID = it->second.dest;
			neighbor.port = 0; 
			neighbor.cost = 1; // for now, same cost

			// Index by HID if neighbor in same domain or by AD otherwise
			route_state.ADNeighborTable[neighbor.AD] = neighbor;

			// Update network table
			std::string myAD = route_state.myAD;

			NodeStateEntry entry;
			entry.ad = myAD;
			entry.hid = myAD;
			entry.num_neighbors = route_state.ADNeighborTable.size();

			// Add neighbors to network table entry
			std::map<std::string, NeighborEntry>::iterator it;
			for (it = route_state.ADNeighborTable.begin(); it != route_state.ADNeighborTable.end(); it++)
				entry.neighbor_list.push_back(it->second);

			route_state.ADNetworkTable[myAD] = entry;
		}
	}
	return 1;
}

void populateNeighboringADBorderRouterEntries(string currHID, std::map<std::string, RouteEntry> &routingTable)
{
	vector<NeighborEntry> currNeighborTable = route_state.networkTable[currHID].neighbor_list;

	vector<NeighborEntry>::iterator it;
	for (it = currNeighborTable.begin(); it != currNeighborTable.end(); it++) { 
		if (it->AD != route_state.myAD) {
			// Add HID of border routers of neighboring ADs into routing table
			string neighborHID = it->HID;
			RouteEntry &entry = routingTable[neighborHID];
			entry.dest = neighborHID;
			entry.nextHop = neighborHID;
			entry.port = it->port;
			//entry.flags = 0;
		}
	}
}

void populateADEntries(std::map<std::string, RouteEntry> &routingTable, std::map<std::string, RouteEntry> ADRoutingTable)
{
	std::map<std::string, RouteEntry>::iterator it1;  // Iter for route table
	
	for (it1 = ADRoutingTable.begin(); it1 != ADRoutingTable.end(); it1++) {
		string destAD = it1->second.dest;
		string nextHopAD = it1->second.nextHop;

		RouteEntry &entry = routingTable[destAD];
		entry.dest = destAD;
		entry.nextHop = routingTable[nextHopAD].nextHop;
		entry.port = routingTable[nextHopAD].port;
		entry.flags = routingTable[nextHopAD].flags;
	}
}

void populateRoutingTable(std::string srcHID, std::map<std::string, NodeStateEntry> &networkTable, std::map<std::string, RouteEntry> &routingTable)
{
	std::map<std::string, NodeStateEntry>::iterator it1;  // Iter for network table
	std::vector<NeighborEntry>::iterator it2;             // Iter for neighbor list

	map<std::string, NodeStateEntry> unvisited;  // Set of unvisited nodes

	routingTable.clear();

	// Filter out anomalies
	//@ (When do these appear? Should they not be introduced in the first place? How about SIDs?)	
	for (it1 = networkTable.begin(); it1 != networkTable.end(); it1++) {
		if (it1->second.num_neighbors == 0 || it1->second.ad.empty() || it1->second.hid.empty()) {
			networkTable.erase(it1);
		}
	}

	unvisited = networkTable;

	// Initialize Dijkstra variables for all nodes
	for (it1=networkTable.begin(); it1 != networkTable.end(); it1++) {
		it1->second.checked = false;
		it1->second.cost = 10000000;
	}

	// Visit root node	
	string currXID;
	unvisited.erase(srcHID);
	networkTable[srcHID].checked = true;
	networkTable[srcHID].cost = 0;

	// Process neighboring nodes of root node
	for (it2 = networkTable[srcHID].neighbor_list.begin(); it2 < networkTable[srcHID].neighbor_list.end(); it2++) {
		currXID = (it2->AD == route_state.myAD) ? it2->HID : it2->AD;

		if (networkTable.find(currXID) != networkTable.end()) {
			networkTable[currXID].cost = it2->cost;
			networkTable[currXID].prevNode = srcHID;
		}
		else {
			// We have an endhost
			NeighborEntry neighbor;
			neighbor.AD = route_state.myAD;
			neighbor.HID = srcHID;
			neighbor.port = 0; // Endhost only has one port
			neighbor.cost = 1;

			NodeStateEntry entry;
			entry.ad = it2->AD;
			entry.hid = it2->HID;
			entry.num_neighbors = 1;
			entry.neighbor_list.push_back(neighbor);
			entry.cost = neighbor.cost;
			entry.prevNode = neighbor.HID;

			networkTable[currXID] = entry;
		}
	}

	// Loop until all nodes have been visited
	while (!unvisited.empty()) {
		int minCost = 10000000;
		string selectedHID;
		// Select unvisited node with min cost
		for (it1=unvisited.begin(); it1 != unvisited.end(); it1++) {
			currXID = (it1->second.ad == route_state.myAD) ? it1->second.hid : it1->second.ad;
			if (networkTable[currXID].cost < minCost) {
				minCost = networkTable[currXID].cost;
				selectedHID = currXID;
			}
		}
		if(selectedHID.empty()) {
			// Rest of the nodes cannot be reached from the visited set
			return;
		}

		// Remove selected node from unvisited set
		unvisited.erase(selectedHID);
		networkTable[selectedHID].checked = true;

		// Process all unvisited neighbors of selected node
		for (it2 = networkTable[selectedHID].neighbor_list.begin(); it2 != networkTable[selectedHID].neighbor_list.end(); it2++) {
			currXID = (it2->AD == route_state.myAD) ? it2->HID : it2->AD;
			if (networkTable[currXID].checked != true) {
				if (networkTable[currXID].cost > networkTable[selectedHID].cost + 1) {
					//@ Why add 1 to cost instead of using link cost from neighbor_list?
					networkTable[currXID].cost = networkTable[selectedHID].cost + 1;
					networkTable[currXID].prevNode = selectedHID;
				}
			}
		}
	}

	// For each destination ID, find the next hop ID and port by going backwards along the Dijkstra graph
	string tempHID1;			// ID of destination in srcHID's routing table
	string tempHID2;			// ID of node currently being processed
	string tempNextHopHID2;		// HID of next hop to reach destID from srcHID
	int hop_count;

	for (it1 = networkTable.begin(); it1 != networkTable.end(); it1++) {
		tempHID1 = (it1->second.ad == route_state.myAD) ? it1->second.hid : it1->second.ad;
		if (tempHID1.find(string("SID")) != string::npos) {
			// Skip SIDs on first pass
			continue;
		}
		if (srcHID.compare(tempHID1) != 0) {
			tempHID2 = tempHID1;
			tempNextHopHID2 = it1->second.hid;
			hop_count = 0;
			while (networkTable[tempHID2].prevNode.compare(srcHID)!=0 && hop_count < MAX_HOP_COUNT) {
				tempHID2 = networkTable[tempHID2].prevNode;
				tempNextHopHID2 = networkTable[tempHID2].hid;
				hop_count++;
			}
			if (hop_count < MAX_HOP_COUNT) {
				routingTable[tempHID1].dest = tempHID1;
				routingTable[tempHID1].nextHop = tempNextHopHID2;
				
				// Find port of next hop
				for (it2 = networkTable[srcHID].neighbor_list.begin(); it2 < networkTable[srcHID].neighbor_list.end(); it2++) {
					if (((it2->AD == route_state.myAD) ? it2->HID : it2->AD) == tempHID2) {
						routingTable[tempHID1].port = it2->port;
					}
				}
			}
		}
	}

	for (it1 = networkTable.begin(); it1 != networkTable.end(); it1++) {
		tempHID1 = (it1->second.ad == route_state.myAD) ? it1->second.hid : it1->second.ad;
		if (tempHID1.find(string("SID")) == string::npos) {
			// Process SIDs on second pass 
			continue;
		}
		if (srcHID.compare(tempHID1) != 0) {
			tempHID2 = tempHID1;
			tempNextHopHID2 = it1->second.hid;
			hop_count = 0;
			while (networkTable[tempHID2].prevNode.compare(srcHID)!=0 && hop_count < MAX_HOP_COUNT) {
				tempHID2 = networkTable[tempHID2].prevNode;
				tempNextHopHID2 = networkTable[tempHID2].hid;
				hop_count++;
			}
			if (hop_count < MAX_HOP_COUNT) {
				routingTable[tempHID1].dest = tempHID1;
				
				// Find port of next hop
				for (it2 = networkTable[srcHID].neighbor_list.begin(); it2 < networkTable[srcHID].neighbor_list.end(); it2++) {
					if (((it2->AD == route_state.myAD) ? it2->HID : it2->AD) == tempHID2) {
						routingTable[tempHID1].port = it2->port;
					}
				}
				
				// Dest is SID, so we search existing ports for entry with same port and HID as next hop
				bool entryFound = false;
				map<string, RouteEntry>::iterator it3;
				for (it3 = routingTable.begin(); it3 != routingTable.end(); it3++) {
					if (it3->second.port == routingTable[tempHID1].port && it3->second.nextHop.find(string("HID")) != string::npos) {
						routingTable[tempHID1].nextHop = it3->second.nextHop;
						entryFound = true;
						break;
					}
				}
				if (!entryFound) {
					// Delete SID entry from routingTable
					routingTable.erase(tempHID1);
				}
			}
		}
	}
	//printRoutingTable(srcHID, routingTable);
}

void printRoutingTable(std::string srcHID, std::map<std::string, RouteEntry> &routingTable)
{
	syslog(LOG_INFO, "Routing table for %s", srcHID.c_str());
	map<std::string, RouteEntry>::iterator it1;
	for ( it1=routingTable.begin() ; it1 != routingTable.end(); it1++ ) {
		syslog(LOG_INFO, "Dest=%s, NextHop=%s, Port=%d, Flags=%u", (it1->second.dest).c_str(), (it1->second.nextHop).c_str(), (it1->second.port), (it1->second.flags) );
	}
}

void printADNetworkTable()
{
	syslog(LOG_INFO, "Network table for %s:", route_state.myAD);
	std::map<std::string, NodeStateEntry>::iterator it;
	for (it = route_state.ADNetworkTable.begin();
		   	it != route_state.ADNetworkTable.end(); it++) {
		syslog(LOG_INFO, "%s", it->first.c_str());
		for (size_t i = 0; i < it->second.neighbor_list.size(); i++) {
			syslog(LOG_INFO, "neighbor[%d]: %s", (int) i,
					it->second.neighbor_list[i].AD.c_str());
		}
	}

}

void initRouteState()
{
	// make the dest DAG (broadcast to other routers)
	Graph g = Node() * Node(BHID) * Node(SID_XROUTE);
	g.fill_sockaddr(&route_state.ddag);

	syslog(LOG_INFO, "xroute Broadcast DAG: %s", g.dag_string().c_str());

	// read the localhost AD and HID
	if ( XreadLocalHostAddr(route_state.sock, route_state.myAD, MAX_XID_SIZE, route_state.myHID, MAX_XID_SIZE, route_state.my4ID, MAX_XID_SIZE) < 0 ) {
		syslog(LOG_ALERT, "Unable to read local XIA address");
		exit(-1);
	}

	// make the src DAG (the one the routing process listens on)
	struct addrinfo *ai;
	if (Xgetaddrinfo(NULL, SID_XROUTE, NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to create source DAG");
		exit(-1);
	}
	memcpy(&route_state.sdag, ai->ai_addr, sizeof(sockaddr_x));

	route_state.num_neighbors = 0; // number of neighbor routers
	route_state.lsa_seq = 0;	// LSA sequence number of this router
	route_state.hello_seq = 0;  // hello seq number of this router
	route_state.sid_discovery_seq = 0;  // sid discovery seq number of this router
	route_state.hello_lsa_ratio = (int32_t) ceil(AD_LSA_INTERVAL/HELLO_INTERVAL);
	route_state.hello_sid_discovery_ratio = (int32_t) ceil(SID_DISCOVERY_INTERVAL/HELLO_INTERVAL);
	route_state.calc_dijstra_ticks = 0;

	route_state.ctl_seq = 0;	// LSA sequence number of this router

	route_state.dual_router_AD = "NULL";
	// mark if this is a dual XIA-IPv4 router
	if( XisDualStackRouter(route_state.sock) == 1 ) {
		route_state.dual_router = 1;
		syslog(LOG_DEBUG, "configured as a dual-stack router");
	} else {
		route_state.dual_router = 0;
	}

	// set timer for HELLO/LSA
	signal(SIGALRM, timeout_handler);
	ualarm((int)ceil(HELLO_INTERVAL*1000000),0); 	
}

void help(const char *name)
{
	printf("\nusage: %s [-l level] [-v] [-c config] [-h hostname]\n", name);
	printf("where:\n");
	printf(" -l level    : syslog logging level 0 = LOG_EMERG ... 7 = LOG_DEBUG (default=3:LOG_ERR)");
	printf(" -v          : log to the console as well as syslog");
	printf(" -h hostname : click device name (default=controller0)\n");
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

	// load local SIDs
    set_sid_conf(hostname);

	// note: ident must exist for the life of the app
	ident = (char *)calloc(strlen(hostname) + strlen (APPNAME) + 4, 1);
	sprintf(ident, "%s:%s", APPNAME, hostname);
	openlog(ident, LOG_CONS|LOG_NDELAY|LOG_LOCAL4|verbose, LOG_LOCAL4);
	setlogmask(LOG_UPTO(level));
}

void set_sid_conf(const char* myhostname)
{
    // read the controller file at etc/controller$i_sid.ini
    // the file defines which AD has what SIDs
    // ini style file
    // the file could be loaded frequently to emulate dynamic service changes

    char full_path[BUF_SIZE];
    char root[BUF_SIZE];
    char section_name[BUF_SIZE];
    char controller_addr[BUF_SIZE];
    char sid[BUF_SIZE];
    std::string service_sid;

    int section_index = 0;
    snprintf(full_path, BUF_SIZE, "%s/etc/%s_sid.ini", XrootDir(root, BUF_SIZE), myhostname);
    route_state.LocalSidList.clear(); // clean and reload all
    while (ini_getsection(section_index, section_name, BUF_SIZE, full_path)) //enumerate every section, [$name]
    {
        //fprintf(stderr, "read %s\n", section_name);
        ServiceState service_state;
        ini_gets(section_name, "sid", sid, sid, BUF_SIZE, full_path);
        service_sid = std::string(sid);
        service_state.capacity = ini_getl(section_name, "capacity", 100, full_path);
        service_state.capacity_factor = ini_getl(section_name, "capacity_factor", 1, full_path);
        service_state.link_factor = ini_getl(section_name, "link_factor", 0, full_path);
        service_state.priority = ini_getl(section_name, "priority", 1, full_path);
        service_state.isController = ini_getbool(section_name, "isController", 0, full_path);
        // get the address of the service controller
        ini_gets(section_name, "controllerAddr", controller_addr, controller_addr, BUF_SIZE, full_path);
        service_state.controllerAddr = std::string(controller_addr);
        if (service_state.isController) // I am the controller
        {
            if (route_state.LocalServiceControllers.find(service_sid) == route_state.LocalServiceControllers.end())
            { // no service controller yet, create one
                ServiceController sc;
                route_state.LocalServiceControllers[service_sid] = sc; // just initialize it
            }
        }
        service_state.archType = ini_getl(section_name, "archType", 0, full_path);
        //fprintf(stderr, "read state%s, %d, %d, %s\n", sid, service_state.capacity, service_state.isController, service_state.controllerAddr.c_str());
        route_state.LocalSidList[service_sid] = service_state;
        section_index++;
    }

    return;
}

int main(int argc, char *argv[])
{
	int rc;
	int selectRetVal, n;
	//size_t found, start;
	socklen_t dlen;
	char recv_message[10240];
	sockaddr_x theirDAG;
	fd_set socks;
	struct timeval timeoutval;
	vector<string> routers;

	config(argc, argv);
	syslog(LOG_NOTICE, "%s started on %s", APPNAME, hostname);

    // connect to the click route engine
	if ((rc = xr.connect()) != 0) {
		syslog(LOG_ALERT, "unable to connect to click (%d)", rc);
		return -1;
	}

	xr.setRouter(hostname);

	// open socket for route process
	route_state.sock=Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (route_state.sock < 0) {
		syslog(LOG_ALERT, "Unable to create a socket");
		exit(-1);
	}

	// initialize the route states (e.g., set HELLO/LSA timer, etc)
	initRouteState();

	// bind to the src DAG
	if (Xbind(route_state.sock, (struct sockaddr*)&route_state.sdag, sizeof(sockaddr_x)) < 0) {
		Graph g(&route_state.sdag);
		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
		Xclose(route_state.sock);
		exit(-1);
	}

	// open socket for controller service
	int32_t tempSock = Xsocket(AF_XIA, SOCK_DGRAM, 0);
	if (tempSock < 0) {
		syslog(LOG_ALERT, "Unable to create a socket");
		exit(-1);
	}

	// bind to the controller service 
	struct addrinfo *ai;
	sockaddr_x tempDAG;
#if 0
	Graph bindG = Node() * Node(route_state.myAD) * Node(SID_XCONTROL);
	bindG.fill_sockaddr(&tempDAG);
#else
	if (Xgetaddrinfo(NULL, SID_XCONTROL, NULL, &ai) != 0) {
		syslog(LOG_ALERT, "unable to bind controller service");
		exit(-1);
	}
	memcpy(&tempDAG, ai->ai_addr, sizeof(sockaddr_x));
#endif

	if (Xbind(tempSock, (struct sockaddr*)&tempDAG, sizeof(sockaddr_x)) < 0) {
		Graph g(&tempDAG);
		syslog(LOG_ALERT, "unable to bind to local DAG : %s", g.dag_string().c_str());
perror("bind");
		Xclose(tempSock);
		exit(-1);
	}

	int sock;
	time_t last_purge = time(NULL);
	while (1) {
		if (route_state.send_hello == true) {
			route_state.send_hello = false;
			sendHello();
		}
		if (route_state.send_lsa == true) {
			route_state.send_lsa = false;
			sendInterdomainLSA();
		}
		if (route_state.send_sid_discovery == true) {
            route_state.send_sid_discovery = false;
            sendSidDiscovery();
        }
        if (route_state.send_sid_decision == true) {
            route_state.send_sid_decision = false;
            processSidDecision();
        }
		FD_ZERO(&socks);
		FD_SET(route_state.sock, &socks);
		FD_SET(tempSock, &socks);
		timeoutval.tv_sec = 0;
		timeoutval.tv_usec = 2000; // every 0.002 sec, check if any received packets

		int32_t highSock = max(route_state.sock, tempSock);
		selectRetVal = select(highSock+1, &socks, NULL, NULL, &timeoutval);
		if (selectRetVal > 0) {
			// receiving a Hello or LSA packet
			memset(&recv_message[0], 0, sizeof(recv_message));
			dlen = sizeof(sockaddr_x);
			if (FD_ISSET(route_state.sock, &socks)) {
				sock = route_state.sock;
			} else if (FD_ISSET(tempSock, &socks)) {
				sock = tempSock;
			} else {
				continue;
			}
			n = Xrecvfrom(sock, recv_message, 10240, 0, (struct sockaddr*)&theirDAG, &dlen);
			if (n < 0) {
				perror("recvfrom");
			}

			string msg = recv_message;
            processMsg(msg);
		}

		time_t now = time(NULL);
		if (now - last_purge >= EXPIRE_TIME)
		{
			last_purge = now;
			fprintf(stderr, "checking entry\n");
			map<string, time_t>::iterator iter = timeStamp.begin();

            while (iter != timeStamp.end())
            {
                if (now - iter->second >= EXPIRE_TIME){
                    xr.delRoute(iter->first);
                    syslog(LOG_INFO, "purging host route for : %s", iter->first.c_str());
                    timeStamp.erase(iter++);
                } else {
                    ++iter;
                }
            }
		}
	}

	return 0;
}
