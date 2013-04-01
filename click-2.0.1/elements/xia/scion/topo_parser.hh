/*****************************************
 * File Name : topo_parser.hh

 * Author : Sangjae Yoo <sangjaey@gmail.com>

 * Date : 24-03-2012

 * Purpose : 

******************************************/
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
	HostAddr addr;
	uint64_t aid; //aid would expedite packet processing in the SCION only network.
};

struct ServerElem: SCIONElem {
		uint8_t type;
};

struct RouterElem: SCIONElem {
		uint16_t interfaceID;
		uint64_t neighbor; //neighbor aid
		uint8_t ntype; //neighbor type
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
    
    public:
    int getNumRouters(){ return m_iNumRouters;}
    int getNumServers(){ return m_iNumServers;}
    int getNumClients(){ return m_iNumClients;}
    int getNumGateways(){ return m_iNumGateways;}
    int loadTopoFile(const char* path);
    int parseServers(multimap<int, ServerElem> &servers); 
    int parseRouters(multimap<int, RouterElem> &routers);
    int parseGateways(multimap<int, GatewayElem> &gateways);
	int parseAddress(XMLElement * ptr, SCIONElem * elem);
    int parseClients(map<int, ClientElem> &clients);
    int parseIFID2AID(map<uint16_t, uint64_t> &i2amap);
    int parseIFID2AID(map<uint16_t, HostAddr> &i2amap);
    
};
#endif
