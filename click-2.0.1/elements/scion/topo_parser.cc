/*
** Copyright 2013 Carnegie Mellon University / ETH Zurich
** 
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
** 
** http://www.apache.org/licenses/LICENSE-2.0 
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<iostream>
#include<sstream>
#include"topo_parser.hh"
#include <click/config.h>
#include <errno.h>
#include <arpa/inet.h>

CLICK_DECLS
using namespace std;
using namespace tinyxml2;

/*
	TopoParser::loadTopoFile
	read a topology file 
*/
int TopoParser::loadTopoFile(const char* path){
    if(doc.LoadFile(path)){
        printf("Error Loading File: %s\n",path);
        return TopoParseFail;
    }
    m_bIsInit = true;
    return TopoParseSuccess;
}

/* 
	TopoParser::parseServers
	construct a server list from the topology file
*/
int TopoParser::parseServers(multimap<int, ServerElem> &servers){
    m_iNumServers=0;
    if(!m_bIsInit){
        printf("Topology file has not been opened yet\n");
        return TopoParseFail;
    }
    XMLElement *ptr = doc.RootElement()->FirstChildElement("Servers");
    XMLElement *ptr2 = ptr->FirstChildElement();
    
    ServerElem newServer; 

	while(ptr2!=NULL){
		//every SCION element has AID (which is interpreted in the SCION network)
		newServer.aid = parseAID(ptr2);

		if(parseAddress(ptr2,(SCIONElem *)&newServer) == TopoParseFail){
        	ptr2=ptr2->NextSiblingElement();
			continue;
		}

        if(!strcmp(ptr2->Name(), "BeaconServer")){
            newServer.type=BeaconServer;
            servers.insert(pair<int, ServerElem>(BeaconServer, newServer));
        }else if(!strcmp(ptr2->Name(), "CertificateServer")){
            newServer.type=CertificateServer;
            servers.insert(pair<int, ServerElem>(CertificateServer, newServer));
			//SL: uncommented to attach CS
			//Note: this kind of temporarily commented out blocks should be marked
			//otherwise, it's hard for others to work with this file...
        }else if(!strcmp(ptr2->Name(), "PathServer")){
            newServer.type=PathServer;
            servers.insert(pair<int, ServerElem>(PathServer, newServer));
        }else{
            printf("Unknown server type=%s\n",ptr2->Name());
            return TopoParseFail;
        }
        m_iNumServers++;
        ptr2=ptr2->NextSiblingElement();
    }

    return TopoParseSuccess;
}

/* 
	TopoParser::parseClients
	construct a client (which is an experimental element) list from the topology file
*/
int TopoParser::parseClients(map<int, ClientElem> &clients){
    m_iNumClients = 0;
    if(!m_bIsInit){
        printf("Topology file has not been opened yet\n");
        return TopoParseFail;
    }
    
    XMLElement *ptr = doc.RootElement()->FirstChildElement("Clients");
    if(ptr==NULL){
        return TopoParseFail;
    }
    XMLElement *ptr2 = ptr->FirstChildElement("Client");
    
	while(ptr2!=NULL){
    	ClientElem newClient;
		
		//every SCION element has AID (which is interpreted in the SCION network)
		newClient.aid = parseAID(ptr2);

		if(parseAddress(ptr2,(SCIONElem *)&newClient) == TopoParseFail){
     		ptr2=ptr2->NextSiblingElement(); 
			continue;
		}

     	clients.insert(pair<int, ClientElem>(m_iNumClients, newClient));
     	m_iNumClients++;
     	ptr2=ptr2->NextSiblingElement(); 
    }
}

int TopoParser::parseRouters(multimap<int, RouterElem> &routers){
    m_iNumRouters=0;
    if(!m_bIsInit){
        printf("Topology file has not been opened yet\n");
        return TopoParseFail;
    }

    XMLElement *ptr, *ptr2 = NULL;
    ptr = doc.RootElement()->FirstChildElement("BorderRouters");

	if(ptr)
    	ptr2 = ptr-> FirstChildElement();
    
	while(ptr2!=NULL){
    
        RouterElem router;    
		
		//every SCION element has AID (which is interpreted in the SCION network)
		router.aid = parseAID(ptr2);
		
		if(parseAddress(ptr2, (SCIONElem *)&router) == TopoParseFail){
        	ptr2=ptr2->NextSiblingElement();
			continue;
		}
        
        m_iNumRouters++;
        XMLElement* ptr3 = ptr2->FirstChildElement("Interface");
       
        while(ptr3!=NULL){
			//SL: changed to a local object instead of allocating memory for the newRouter.
			//the newRouter has only local meaning and is copied to a multimap (whose 
			//elements are defined by a value).
            
            RouterElem newRouter;
			newRouter.addr = router.addr;
			newRouter.aid = router.aid;

            stringstream s;
            s<< ptr3->FirstChildElement("IFID")->GetText()<<" "
                <<ptr3->FirstChildElement("NeighborAD")->GetText();
            s>>newRouter.interface.id;
            s>>newRouter.interface.neighbor;
        
            if(!strcmp(ptr3->FirstChildElement("NeighborType")->GetText(), "PARENT")){
                newRouter.interface.ntype = Parent;
            }else if(!strcmp(ptr3->FirstChildElement("NeighborType")->GetText(), "CHILD")){
                newRouter.interface.ntype = Child;
            }else if(!strcmp(ptr3->FirstChildElement("NeighborType")->GetText(), "PEER")){
                newRouter.interface.ntype = Peer;
            }else{
                printf("Unknown ntype = %s\n",
                ptr3->FirstChildElement("NeighborType")->GetText());
                return TopoParseFail;
            }

			//check if the interface is assigned an address (for IP tunneling).
			//SL: address doesn't need to be assigned if routers are directly connected
			//i.e., two SCION routers can be directly connected without any IP/AIP address
			XMLElement *ptr4 = ptr3->FirstChildElement("AddrType");
			if(ptr4) 
				parseAddress(ptr3, (SCIONElem *)&(newRouter.interface));

			//finally, add to the router list
            routers.insert(pair<int, RouterElem>(newRouter.interface.ntype, newRouter));
            ptr3=ptr3->NextSiblingElement(); 
			#ifdef _SL_DEBUG_RT
			if(ptr4)
				printf("Parser - Router(%llu): IFID: %d, Addr:%d, ToAddr:%d\n", newRouter.aid, newRouter.interface.id,
					newRouter.interface.addr.getIPv4Addr(),newRouter.interface.to_addr.getIPv4Addr());
			#endif 

        }
        ptr2=ptr2->NextSiblingElement();
    }
    return TopoParseSuccess;
}


int TopoParser::parseEgressIngressPairs(multimap<int, EgressIngressPair> &pairmap){

    m_iNumRoutePairs=0;
    if(!m_bIsInit){
        printf("Topology file has not been opened yet\n");
        return TopoParseFail;
    }

    XMLElement *ptr, *ptr2 = NULL;
    ptr = doc.RootElement()->FirstChildElement("RoutePair");

	if(ptr) {
		ptr2 = ptr-> FirstChildElement("RPair");
    }
    
	while(ptr2!=NULL){
			
		EgressIngressPair rpair;
			
		if(!strcmp(ptr2->FirstChildElement("NeighborType")->GetText(), "PARENT")){
			rpair.ntype = Parent;
		}else if(!strcmp(ptr2->FirstChildElement("NeighborType")->GetText(), "CHILD")){
            rpair.ntype = Child;
        }else if(!strcmp(ptr2->FirstChildElement("NeighborType")->GetText(), "PEER")){
            rpair.ntype = Peer;
        }else{
            printf("Unknown ntype = %s\n",
            ptr2->FirstChildElement("NeighborType")->GetText());
            return TopoParseFail;
        }
        
        memcpy(rpair.egress_ad, ptr2->FirstChildElement("EGRESS_AD")->GetText(), 40);
        memcpy(rpair.egress_addr, ptr2->FirstChildElement("EGRESS_HID")->GetText(), 40);
        memcpy(rpair.ingress_ad, ptr2->FirstChildElement("INGRESS_AD")->GetText(), 40);
        memcpy(rpair.ingress_addr, ptr2->FirstChildElement("INGRESS_HID")->GetText(), 40);
        memcpy(rpair.dest_ad, ptr2->FirstChildElement("DEST_AD")->GetText(), 40);
        memcpy(rpair.dest_addr, ptr2->FirstChildElement("DEST_HID")->GetText(), 40);
		
		rpair.egress_ad[40] = rpair.egress_addr[40] 
		= rpair.ingress_ad[40] = rpair.ingress_addr[40] 
		= rpair.dest_ad[40] = rpair.dest_addr[40] 
		= '\0';
		
		pairmap.insert(pair<int, EgressIngressPair>(rpair.ntype, rpair));
			
		m_iNumRoutePairs++;
		ptr2=ptr2->NextSiblingElement();
    }
    return TopoParseSuccess;
}


/*not supported yet*/
int 
TopoParser::parseGateways(multimap<int, GatewayElem> &gateways) {
    if(!m_bIsInit){
        printf("Topology file has not been opened yet\n");
        return TopoParseFail;
    }
    
	XMLElement *ptr = doc.RootElement()->FirstChildElement("Gateways");
    if(ptr==NULL){
        return TopoParseFail;
    }
    
	XMLElement *ptr2 = ptr->FirstChildElement("Gateway");
	#ifdef _SL_DEBUG
	printf("parsing in progress: ptr:%x, prt2:%x, #:%d\n", ptr, ptr2, m_iNumGateways);
	#endif
    while(ptr2!=NULL){
        
		GatewayElem newGateway;
		newGateway.aid = parseAID(ptr2);
		
		if(parseAddress(ptr2, (SCIONElem *)&newGateway) == TopoParseFail){
        	ptr2=ptr2->NextSiblingElement();
			continue;
		}
        
		//SLT:
		//Must be changed to HostAddr type... immediately
		//Currently, SCION elements have 8B SCION addresses.
        gateways.insert(pair<int, GatewayElem>(newGateway.ptype, newGateway));
        m_iNumGateways++;
        ptr2=ptr2->NextSiblingElement();
    }

    return TopoParseSuccess;
}

//SLT: unnecessary
//need to be removed once change to the new packet format is completed
int TopoParser::parseIFID2AID(map<uint16_t, uint64_t> &i2amap){

    if(!m_bIsInit){
        printf("Topology file has not been opened yet\n");
        return TopoParseFail;
    }

    XMLElement *ptr = doc.RootElement()->FirstChildElement("BorderRouters");
    XMLElement *ptr2 = ptr-> FirstChildElement();
    while(ptr2!=NULL){
        const char* aid = ptr2->FirstChildElement("AID")->GetText();
        XMLElement* ptr3 = ptr2->FirstChildElement("Interface");
        while(ptr3!=NULL){
            stringstream s;
            s<<aid<<" "<< ptr3->FirstChildElement("IFID")->GetText();
            uint16_t ifid;
            uint64_t rAid;
            s>>rAid;
            s>>ifid;
            i2amap.insert(pair<uint16_t, uint64_t>(ifid, rAid));
            ptr3=ptr3->NextSiblingElement(); 
        }
        ptr2=ptr2->NextSiblingElement();
    }
    return TopoParseSuccess;
    
}

//SJ::: needs to be implemented
/*SL:
construct IFID to AID map to enable switch to make forwarding decision
based on the interface ID in the packet.
*/
int TopoParser::parseIFID2AID(map<uint16_t, HostAddr> &i2amap){
    
	if(!m_bIsInit){
        printf("Topology file has not been opened yet\n");
        return TopoParseFail;
    }
    
	XMLElement *ptr = doc.RootElement()->FirstChildElement("BorderRouters");
    XMLElement *ptr2 = ptr-> FirstChildElement();
    while(ptr2!=NULL){

        RouterElem router;    
		router.aid = parseAID(ptr2);

		if(parseAddress(ptr2, (SCIONElem *)&router) == TopoParseFail){
        	ptr2=ptr2->NextSiblingElement();
			continue;
		}
        
        XMLElement* ptr3 = ptr2->FirstChildElement("Interface");
        while(ptr3!=NULL){
            uint16_t ifid;
            stringstream s;
            s<<ptr3->FirstChildElement("IFID")->GetText();
            s>>ifid;
            i2amap.insert(pair<uint16_t, HostAddr>(ifid, router.addr));
            ptr3=ptr3->NextSiblingElement(); 
        }
        ptr2=ptr2->NextSiblingElement();
    }
    return TopoParseSuccess;
}

uint64_t
TopoParser::parseAID(XMLElement * ptr) {
	uint64_t aid;
    stringstream s;
	s << ptr->FirstChildElement("AID")->GetText();
	s >> aid;
	return aid;
}

/*SL:
	TopoParser::parseAddress
	parse address of an element from the topology file
*/
int
TopoParser::parseAddress(XMLElement * ptr, SCIONElem * elem) {
    uint64_t addr64 = 0;
	uint32_t addr32 = 0;
    uint8_t addrLen = 0; 
    uint8_t addrType = 0;
	string sAddrType;
	string addr_str;

    stringstream s;
	stringstream ta;

	//SLA:
	s << ptr->FirstChildElement("AddrType")->GetText() << " "
    << ptr->FirstChildElement("Addr")->GetText();

    XMLElement* ptr2;
	if((ptr2 = ptr->FirstChildElement("ToAddr")) != NULL)
		ta << ptr2->GetText();
	
	//1. parse address type
	s>>sAddrType;
	for(int l=0; l<sAddrType.length(); l++) {
		sAddrType[l] = std::toupper(sAddrType[l]);
	}

	//2. parse address based on the type
	if(!sAddrType.compare(STR_SCION)){
		s>>addr64;
		elem->addr = HostAddr(HOST_ADDR_SCION,addr64);	
		
		if(ptr2) {
			ta >> addr64;
			elem->to_addr = HostAddr(HOST_ADDR_SCION,addr64);
		}
			
	} else if (!sAddrType.compare(STR_IPV4)) {
		s>>addr_str;
		in_addr a;

		if(!inet_pton(AF_INET,addr_str.c_str(),&a)){
			printf("invalid IPv4 address: %s\n",addr_str.c_str());
			return TopoParseFail;
		}

		elem->addr = HostAddr(HOST_ADDR_IPV4,a.s_addr);

		if(ptr2) {
			ta >> addr_str;
			if(!inet_pton(AF_INET,addr_str.c_str(),&a)){
				printf("invalid IPv4 address: %s\n",addr_str.c_str());
				return TopoParseFail;
			}
			elem->to_addr = HostAddr(HOST_ADDR_IPV4,a.s_addr);
		}

	} else if (!sAddrType.compare(STR_IPV6)) {
		s>>addr_str;
		in6_addr a;

		if(!inet_pton(AF_INET6,addr_str.c_str(),&a)){
			printf("invalid IPv6 address: %s\n",addr_str.c_str());
			return TopoParseFail;
		}
		elem->addr = HostAddr(HOST_ADDR_IPV6,a.s6_addr);

		if(ptr2) {
			ta >> addr_str;
			if(!inet_pton(AF_INET6,addr_str.c_str(),&a)){
				printf("invalid IPv6 address: %s\n",addr_str.c_str());
				return TopoParseFail;
			}
			elem->to_addr = HostAddr(HOST_ADDR_IPV6,a.s6_addr);
		}

	} else if (!sAddrType.compare(STR_AIP)) {
		s>>addr_str;
		elem->addr = HostAddr(HOST_ADDR_AIP,(uint8_t *)addr_str.c_str());

		if(ptr2) {
			ta >> addr_str;
			elem->to_addr = HostAddr(HOST_ADDR_AIP,(uint8_t *)addr_str.c_str());
		}

	} else {
		printf("Unknown address type: %s\n",sAddrType.c_str());
		return TopoParseFail;
	}

	return TopoParseSuccess;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(TOPO_Parser)

