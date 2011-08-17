// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_CLONE_HH
#define CLICK_CLONE_HH
#include <click/element.hh>
#include <click/task.hh>

CLICK_DECLS

class Clone : public Element { public:
    Clone();
    ~Clone();
    const char *class_name() const	{ return "Clone"; }
    const char *port_count() const	{ return "1/1"; }
    const char *processing() const	{ return "h/l"; }
    void push(int, Packet *);
    int configure(Vector<String> &conf, ErrorHandler *errh);
    Packet * pull(int);
    private:
    Vector<Packet *> _packets;
    int _count;
    int _next;
};
CLICK_ENDDECLS
#endif
