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

private:
    uint64_t m_uAid;

    ReadWriteLock _lock;
	String m_sTopologyFile;
};

CLICK_ENDDECLS
#endif
