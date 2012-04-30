#ifndef CLICK_XIASELECTPATH_HH
#define CLICK_XIASELECTPATH_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
 * =c
 * XIASelectPath(MODE mode)
 * mode is "first" or "next".
 *
 * =s ip
 * selects a path to the next XID (i.e., main or next fallback)
 *
 * =d
 * Selects a path to the next XID.  If MODE is first, the main path is selected.  If MODE is next, the next fallback is selected.
 * When this succeeds, output the packet to port 0.
 * If there is no more path to consider, output the packet to port 1.
 * 
 * =a
 */

class XIASelectPath : public Element { public:

    XIASelectPath();
    ~XIASelectPath();

    const char *class_name() const		{ return "XIASelectPath"; }
    const char *port_count() const		{ return "1/2"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &conf, ErrorHandler *errh);

    void push(int port, Packet *);

  private:
    bool _first;
};

CLICK_ENDDECLS
#endif
