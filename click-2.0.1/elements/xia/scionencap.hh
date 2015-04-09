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

#ifndef SCIONENCAP_HH
#define SCIONENCAP_HH

#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
#include <clicknet/ip.h>
#include <list>
#include "scionpathinfo.hh"
#include "scionprint.hh"
#include "topo_parser.hh"
#include "packetheader.hh"

CLICK_DECLS

struct GatewayAddr {
	uint32_t tdid; //TDID where the gateway resides
	uint64_t adaid; //AD AID where the gateway resides
	HostAddr addr;	//Gateway's address
};

struct PacketCache
{
    Packet *p;
    GatewayAddr dst_gw; //destination ADAID
};
    

class SCIONEncap : public Element { 

public:
    SCIONEncap(): m_uAdAid(0), _path_info(0),m_bDefaultDestOn(false),
    m_bClearReversePath(false),_timer(this) {};
    ~SCIONEncap() {
		delete scionPrinter;
	};

    const char *class_name() const { return "SCIONEncap"; }
    const char *port_count() const {return "-/-";}
    const char *processing() const {return PUSH;}

    int configure(Vector<String> &, ErrorHandler *);
	int initialize(ErrorHandler *);
	void run_timer(Timer *);
    void push(int, Packet *);

private:
    String m_AD;
    String m_HID;

	Timer _timer;
	String m_sTopologyFile;
	//String m_sAddrTableFile;
	char m_csLogFile[MAX_FILE_LEN];
	
	int m_iLogLevel;
	
    std::multimap<int, Servers> m_servers;

	uint64_t m_uAid;
	uint64_t m_uAdAid;
	uint32_t m_uTdAid;

	SCIONPrint * scionPrinter;

    void handle_data(Packet *, GatewayAddr &gw, bool);
    void handle_data(Packet *, GatewayAddr &gw, fullPath &path, bool);

    void sendPacket(uint8_t* data, uint16_t data_length, string dest);
    void sendHello();

    //HostAddr _addr; //gateway Addr
	//SL: this is 4B, yet in some 64bit OS, this is 8B
	//better to switch to uint32
	//unsigned long _src;
    SCIONPathInfo *_path_info;
    
	//SL: cache for temporary packet store (before resolving path to the destination AD)
	std::list<PacketCache> _cache;

	//SL: changed to uint32_t
	//map IP address to AD
    //std::map<unsigned long, uint64_t> _ad_table;
    std::map<uint32_t, GatewayAddr> _ad_table;
    std::map<uint32_t, GatewayAddr> _ad_table_24;
	//set default destination for all destination
	bool m_bDefaultDestOn;
	bool m_bClearReversePath;
	GatewayAddr _gw_default;

    ReadWriteLock _lock;
};

CLICK_ENDDECLS
#endif
