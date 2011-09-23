// -*- mode: c++; c-basic-offset: 4 -*-
#ifndef CLICK_BARRIER_HH
#define CLICK_BARRIER_HH
#include <click/element.hh>
#include <click/task.hh>

CLICK_DECLS

class Barrier : public Element { public:
    Barrier();
    ~Barrier();
    const char *class_name() const	{ return "Barrier"; }
    const char *port_count() const	{ return "-/1"; }
    const char *processing() const	{ return "PUSH"; }
    bool run_task(Task *);
    void selected(int,int);
    private:
    Task _task;
};
CLICK_ENDDECLS
#endif
