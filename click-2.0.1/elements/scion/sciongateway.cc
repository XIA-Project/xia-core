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

#include <click/config.h>
#include "sciongateway.hh"
#include "packetheader.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/ip.h>
#include <arpa/inet.h>
#include <fstream>
CLICK_DECLS

SCIONGateway::SCIONGateway() :
m_uAid(0)
{
}

SCIONGateway::~SCIONGateway()
{
}

int
SCIONGateway::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String table_file;
    if (Args(conf, this, errh)
    .read_mp("AID", m_uAid)
    .read_mp("CONFIG_FILE", m_sConfigFile)
    .read_mp("TOPOLOGY_FILE", m_sTopologyFile)
    .complete() < 0)
        return -1;

    return 0;
}

int
SCIONGateway::initialize(ErrorHandler *)
{
    parseTopology();
    constructIfid2AddrMap();
	initializeOutputPort();
    return 0;
}

/*
	SCIONGateway::initializeOutputPort
	prepare IP header for IP encapsulation
	if the port is assigned an IP address
*/
void SCIONGateway::initializeOutputPort() {
	
	portInfo p;
	p.addr = m_Addr;
	m_vPortInfo.push_back(p);

	//Initialize port 0; i.e., prepare internal communication
	if(m_Addr.getType() == HOST_ADDR_IPV4) {
		m_pIPEncap = new SCIONIPEncap;
		m_pIPEncap->initialize(m_Addr.getIPv4Addr());
	}
}


void
SCIONGateway::constructIfid2AddrMap() {
	std::multimap<int, RouterElem>::iterator itr;
	for(itr = m_routers.begin(); itr!=m_routers.end(); itr++) {
		ifid2addr.insert(std::pair<uint16_t,HostAddr>(itr->second.interface.id, itr->second.addr));
	}
}


    // input
    //   0: from SCION Switch (AID Request / Data / Path)
    //   1: from SCION Encap  (Data / Path Request)
    // output
    //   0: to SCION switch (Data / Path Request / AID Reply)
    //   1: to SCION Encap (Path)
    //   2: to SCION Decap (Data)
    //
    // packet flow
    //   input 0: 
    //     aid req -> (locallly handled) -> output 0
    //     data -> output 2
    //     path -> output 1
    //   input 1:
    //     data/path req -> output 0 

/*
    SCIONGateway::parseTopology
    - parse topology file using topo parser
*/
void 
SCIONGateway::parseTopology(){

    TopoParser parser;
    parser.loadTopoFile(m_sTopologyFile.c_str()); 
    parser.parseServers(m_servers);
    parser.parseRouters(m_routers);
    parser.parseGateways(m_gateways);

	std::multimap<int, GatewayElem>::iterator itr;
	for(itr = m_gateways.begin(); itr != m_gateways.end(); itr++)
		if(itr->second.aid == m_uAid){
			m_Addr = itr->second.addr;
			break;
		}
}



void
SCIONGateway::push(int port, Packet *p)
{
    // all packets are SCION..
    unsigned const char *data = p->data();
    uint16_t type = SPH::getType((uint8_t *)data);
    uint16_t data_length = SPH::getTotalLen((uint8_t *)data);

	//1. from Switch
    if (port == 0) {

		if(m_vPortInfo[PORT_TO_SWITCH].addr.getType() == HOST_ADDR_IPV4){
			struct ip * p_iph = (struct ip *)data;
			if(p_iph->ip_p != SCION_PROTO_NUM) {
				p->kill();
				return;
			}
			data += IPHDR_LEN;
		}

        if (type == DATA) {
            output(2).push(p);

        } else if (type == AID_REQ) {
            WritablePacket *q = p->uniqueify();
            uint8_t *data_out = q->data();
            uint64_t *ptr = (uint64_t*)(data_out + COMMON_HEADER_SIZE);
            *ptr = m_uAid;
            
            SPH::setType((uint8_t *)data_out, AID_REP);
			HostAddr addr(HOST_ADDR_SCION,m_uAid);
       		SPH::setSrcAddr((uint8_t *)data_out, addr);
            
            output(0).push(q);

        } else if (type == UP_PATH || type == PATH_REP_LOCAL) {
			#ifdef _SL_DEBUG_GW
			printf("Path reply from PS: Addr: %llu\n", m_uAid);
			#endif
            output(1).push(p);
        } else {
            p->kill();
            click_chatter("unexpected packet type.");
        }
	//2. from encap element
    } else {

        if (type == DATA || type == PATH_REQ_LOCAL) {
			#ifdef _SL_DEBUG_GW
			if(type == PATH_REQ_LOCAL) {
				HostAddr srcAddr = SPH::getSrcAddr((uint8_t *)data);
				HostAddr dstAddr = SPH::getDstAddr((uint8_t *)data);
				printf("Path request to PS: GW Addr:%llu, srcAddr: %llu, dstAddr: %llu\n", 
					m_uAid, srcAddr.numAddr(), dstAddr.numAddr());
			}
			#endif
			//SLA:
			uint8_t ipp[data_length+IPHDR_LEN]; 
			if(m_vPortInfo[PORT_TO_SWITCH].addr.getType() == HOST_ADDR_IPV4) {
				switch(type) {
				case PATH_REQ_LOCAL:
				if(m_pIPEncap->encap(ipp,(uint8_t *)data,data_length,SPH::getDstAddr((uint8_t *)data).getIPv4Addr()) 
					== SCION_FAILURE)
					return;
				break;
				case DATA:{
				uint16_t iface = SPH::getOutgoingInterface((uint8_t *)data);
				std::map<uint16_t,HostAddr>::iterator itr = ifid2addr.find(iface);
				if(itr == ifid2addr.end()) return;
				if(m_pIPEncap->encap(ipp,(uint8_t *)data,data_length,itr->second.getIPv4Addr()) == SCION_FAILURE)
					return;
				} break;
				default: break;
				}
				p->kill();
    			WritablePacket* outPacket= Packet::make(DEFAULT_HD_ROOM, ipp, data_length, DEFAULT_TL_ROOM);
            	output(0).push(outPacket);
			} else {
            	output(0).push(p);
			}
        } else {
            click_chatter("unexpected packet type %d", type);
        }
    }

}

CLICK_ENDDECLS
EXPORT_ELEMENT(SCIONGateway)
ELEMENT_MT_SAFE(SCIONGateway)
