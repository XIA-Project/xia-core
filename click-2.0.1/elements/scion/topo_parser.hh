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

#ifndef TOPO_PARSER_HH_
#define TOPO_PARSER_HH_
#include<string.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<map>
#include"tinyxml2.hh"
#include "packetheader.hh"

using namespace std;
using namespace tinyxml2;


enum TopologyError{
    TopoParseSuccess=0,
    TopoParseFail,
};

enum Type{
    BeaconServer=0,
    CertificateServer,
    PathServer,
    BorderRouter,
    Gateway,
};

enum NType{
    Parent=0,
    Child,
    Peer,
};

enum Protocol{
    Default=0,
};

struct SCIONElem {
	uint64_t aid; 	//aid would expedite packet processing in the SCION only network.
	HostAddr addr; 	//in SCION network, aid == addr
	HostAddr to_addr; 	//when destination is fixed; 
						//e.g., Border Router - Border Router connection
};

struct ServerElem: SCIONElem {
	uint8_t type;
};

struct InterfaceElem: SCIONElem {
	uint16_t id;
	uint64_t neighbor; //neighbor aid
	uint8_t ntype; //neighbor type
	InterfaceElem& operator=(const InterfaceElem &rhs) {
		id = rhs.id;
		neighbor = rhs.neighbor;
		ntype = rhs.ntype;
		aid = rhs.aid;
		addr = rhs.addr;
		to_addr = rhs.to_addr;
		return *this;
	}
};

struct RouterElem: SCIONElem {
	//uint16_t interfaceID;
	//uint64_t neighbor; //neighbor aid
	//uint8_t ntype; //neighbor type
	//SL:
	//The above variables need to be deleted (soon)...
	InterfaceElem interface;
	RouterElem& operator=(const RouterElem& rhs) {
		aid = rhs.aid;
		addr = rhs.addr;
		to_addr = rhs.to_addr;
		interface = rhs.interface;
		return *this;
	}
};

struct GatewayElem: SCIONElem {
		uint8_t ptype;
};

struct ClientElem: SCIONElem {
};

//SL: need to use singular (a bit confused)
struct Servers{
    uint8_t type;
    uint64_t aid;
    HostAddr addr;
};

//SL: why router has a single interface?
struct Routers{
	HostAddr addr;
    uint64_t aid;
    uint16_t interfaceID;
    uint64_t neighbor;
    uint8_t ntype;
};

struct Gateways{
    uint8_t ptype;
    uint64_t aid;
	HostAddr addr;
};

struct Clients{
	HostAddr addr;
    uint64_t aid;
};
/////////////////////////////////////////////////


class TopoParser{
    public : 
    TopoParser(){
       m_bIsInit = false;
       m_iNumRouters =0;
       m_iNumServers=0;
       m_iNumGateways=0; 
       m_iNumClients=0;
    }
    
    
    private:
    bool m_bIsInit; 
    int m_iNumRouters;
    int m_iNumServers;
    int m_iNumGateways;
    int m_iNumClients;
    XMLDocument doc;

	int parseAddress(XMLElement * ptr, SCIONElem * elem);
	uint64_t parseAID(XMLElement * ptr);
    
    public:
    /** @brief Returns the number of routers */
    int getNumRouters(){ return m_iNumRouters;}
    /** @brief Returns the number of servers*/
    int getNumServers(){ return m_iNumServers;}
    /** @brief Returns the number of clients */
    int getNumClients(){ return m_iNumClients;}
    /** @brief Returns the number of gateways */
    int getNumGateways(){ return m_iNumGateways;}
    /** @brief Initializes the topo_parser object 
        This function MUST be called before calling any
        of the functions for topo_parser. 
    */
    int loadTopoFile(const char* path);
    /**
        @brief puts all the servers in the given servers list
    */
    int parseServers(multimap<int, ServerElem> &servers); 
    /**
        @brief puts all the routers in the given routers list
    */
    int parseRouters(multimap<int, RouterElem> &routers);
    /**
        @brief puts all the gateways in the given gateways list
    */
    int parseGateways(multimap<int, GatewayElem> &gateways);
    int parseClients(map<int, ClientElem> &clients);
    int parseIFID2AID(map<uint16_t, uint64_t> &i2amap);
    int parseIFID2AID(map<uint16_t, HostAddr> &i2amap);
    
};
#endif
