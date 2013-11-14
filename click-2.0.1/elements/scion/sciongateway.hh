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

#ifndef SCIONGATEWAY_HH
#define SCIONGATEWAY_HH

#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
#include <clicknet/ip.h>
#include <list>
#include "scionpathinfo.hh"
#include "scionipencap.hh"
#include "define.hh"
#include "packetheader.hh"
#include "topo_parser.hh"

CLICK_DECLS

class SCIONGateway : public Element { 

public:
    SCIONGateway();
    ~SCIONGateway();

    const char *class_name() const        { return "SCIONGateway"; }

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

    //const char *port_count() const        { return "2/3"; }
	//SLT: temporarily for test...
    const char *port_count() const        { return "2/3"; }
    const char *processing() const        { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    void push(int, Packet *);

    void parseTopology();
    void constructIfid2AddrMap();
	void initializeOutputPort();

private:
    uint64_t m_uAid;
	HostAddr m_Addr;

    ReadWriteLock _lock;
	String m_sTopologyFile;
	String m_sConfigFile;

	//SL: For IP tunneling
	SCIONIPEncap * m_pIPEncap;
	vector<portInfo> m_vPortInfo;	//address type and address of each interface
    std::multimap<int, ServerElem> m_servers;
    std::multimap<int, RouterElem> m_routers;
    std::multimap<int, GatewayElem> m_gateways;         
    std::map<uint16_t, HostAddr> ifid2addr;

};

CLICK_ENDDECLS
#endif
