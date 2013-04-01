#ifndef SCIONDECAP_HH
#define SCIONDECAP_HH

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

class SCIONDecap : public Element { 

public:
    SCIONDecap();
    ~SCIONDecap();

    const char *class_name() const        { return "SCIONDecap"; }

    // input
    //   0: from SCION network (data plane & control plane)
    // output
    //   0: to SCION switch (data plane)
    
    const char *port_count() const        { return "1/1"; }
    const char *processing() const        { return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    Packet *simple_action(Packet *);

private:
	SCIONPathInfo *_path_info;    
};

CLICK_ENDDECLS
#endif
