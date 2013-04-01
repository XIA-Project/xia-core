/*****************************************
 * File Name : topo_parser.cc

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 24-03-2012

 * Purpose : 

******************************************/

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
        printf("Error Loading File\n");
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

    XMLElement *ptr = doc.RootElement()->FirstChildElement("BorderRouters");
    XMLElement *ptr2 = ptr-> FirstChildElement();
    while(ptr2!=NULL){
    
        RouterElem router;    
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
            s>>newRouter.interfaceID;
            s>>newRouter.neighbor;
        
            if(!strcmp(ptr3->FirstChildElement("NeighborType")->GetText(), "PARENT")){
                newRouter.ntype = Parent;
            }else if(!strcmp(ptr3->FirstChildElement("NeighborType")->GetText(), "CHILD")){
                newRouter.ntype = Child;
            }else if(!strcmp(ptr3->FirstChildElement("NeighborType")->GetText(), "PEER")){
                newRouter.ntype = Peer;
            }else{
                printf("Unknown ntype = %s\n",
                ptr3->FirstChildElement("NeighborType")->GetText());
                return TopoParseFail;
            }
            routers.insert(pair<int, RouterElem>(newRouter.ntype, newRouter));
            ptr3=ptr3->NextSiblingElement(); 
        }
        ptr2=ptr2->NextSiblingElement();
    }
    return TopoParseSuccess;
}


/*not supported yet*/
int TopoParser::parseGateways(multimap<int, GatewayElem> &gateways){
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
	char buf[20];

    stringstream s;
	//SLA:
	s << ptr->FirstChildElement("AddrType")->GetText() << " "
    << ptr->FirstChildElement("Addr")->GetText();
	
	//1. parse address type
	s>>sAddrType;
	for(int l=0; l<sAddrType.length(); l++) {
		sAddrType[l] = std::toupper(sAddrType[l]);
	}

	//2. parse address based on the type
	if(!sAddrType.compare(STR_SCION)){
		s>>addr64;
		elem->addr = HostAddr(HOST_ADDR_SCION,addr64);	
	} else if (!sAddrType.compare(STR_IPV4)) {
		s>>addr_str;
		in_addr a;

		if(!inet_pton(AF_INET,addr_str.c_str(),&a)){
			printf("invalid IPv4 address: %s\n",addr_str.c_str());
			return TopoParseFail;
		}
		elem->addr = HostAddr(HOST_ADDR_IPV4,a.s_addr);

	} else if (!sAddrType.compare(STR_IPV6)) {
		s>>addr_str;
		in6_addr a;

		if(!inet_pton(AF_INET6,addr_str.c_str(),&a)){
			printf("invalid IPv4 address: %s\n",addr_str.c_str());
			return TopoParseFail;
		}
		elem->addr = HostAddr(HOST_ADDR_IPV6,a.s6_addr);

	} else if (!sAddrType.compare(STR_AIP)) {
		s>>addr_str;
		elem->addr = HostAddr(HOST_ADDR_AIP,(uint8_t *)addr_str.c_str());

	} else {
		printf("Unknown address type: %s\n",sAddrType.c_str());
		return TopoParseFail;
	}

	return TopoParseSuccess;
}

CLICK_ENDDECLS
ELEMENT_PROVIDES(TOPO_Parser)

