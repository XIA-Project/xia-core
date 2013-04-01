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
    SCIONEncap();
    ~SCIONEncap() {
		delete scionPrinter;
	};

    const char *class_name() const        { return "SCIONEncap"; }

    // input
    //   0: from IP network (data plane)
    //   1: from SCION switch (control plane)
    // output
    //   0: to SCION switch (data plane)
    //const char *port_count() const        { return "2/1"; }
    const char *port_count() const        { return "2/1"; }
    const char *processing() const        { return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);
	void initVariables();

    void push(int, Packet *);
	void parseTopology();
    void constructIfid2AddrMap(); 

private:
	String m_sConfigFile;
	String m_sTopologyFile;
	String m_sAddrTableFile;
	char m_csLogFile[MAX_FILE_LEN];
	
	int m_iLogLevel;
	

    std::multimap<int, ServerElem> m_servers;
    std::multimap<int, RouterElem> m_routers;
    std::map<uint16_t, HostAddr> ifid2addr;

	uint64_t m_uAid;
	uint64_t m_uAdAid;
	uint32_t m_uTdAid;

	SCIONPrint * scionPrinter;

    void handle_data(Packet *, GatewayAddr &gw, bool);
    void handle_data(Packet *, GatewayAddr &gw, fullPath &path, bool);

    HostAddr _addr; //gateway Addr
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

    ReadWriteLock _lock;
};

CLICK_ENDDECLS
#endif
