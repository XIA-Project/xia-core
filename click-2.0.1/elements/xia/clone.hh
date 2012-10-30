// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_CLONE_HH
#define CLICK_CLONE_HH
#include <click/element.hh>
#include <click/task.hh>

CLICK_DECLS

/*
 * =c
 * Clone([COUNT], [SHARED_SKBS], [WAITUNTIL], [ACTIVE])
 *
 * =s ip
 * clones packets
 *
 * =d
 *
 * Clones input packets and supply at output ports.  Works as if it were a Queue.
 *
 * =item COUNT
 *
 * Specifies the number of packets to clone.  After cloning this many packets, it will return no further packets
 *
 * =item SHARED_SKBS
 *
 * Tries to use the shared SKB struct in the cloned packets.
 *
 * =item WAITUNTIL
 *
 * Waits for the specified number of packets at the input port before returning any packets at the output port
 *
 * =item ACTIVE
 *
 * Specifies if this element is active (cloning packets).
 *
 * =a
 */

class Clone : public Element { public:
    Clone();
    ~Clone();
    const char *class_name() const	{ return "Clone"; }
    const char *port_count() const	{ return "1/1"; }
    const char *processing() const	{ return "h/l"; }

    void push(int, Packet *);
    int configure(Vector<String> &conf, ErrorHandler *errh);
    Packet * pull(int);

    void add_handlers();
    static int set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh);

  private:
    Vector<Packet *> _packets;
    int _count;
    int _next;
    bool _shared_skbs;
    int _wait_until;
    bool _active;
};
CLICK_ENDDECLS
#endif
