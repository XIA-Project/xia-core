// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_CLONE_HH
#define CLICK_CLONE_HH
#include <click/element.hh>
#include <click/task.hh>
#include <click/notifier.hh>
#include <click/ipflowid.hh>

CLICK_DECLS

class Clone : public Element { public:
    Clone();
    ~Clone();
    const char *class_name() const	{ return "Clone"; }
    const char *port_count() const	{ return "1/1"; }
    const char *processing() const	{ return "h/l"; }
    void push(int, Packet *);
    Packet * pull(int);
    private:
    Packet *_packet;
};
CLICK_ENDDECLS
#endif
