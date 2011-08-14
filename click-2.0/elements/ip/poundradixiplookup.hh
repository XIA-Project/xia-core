#ifndef CLICK_POUNDRADIXIPLOOKUP_HH
#define CLICK_POUNDRADIXIPLOOKUP_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS
class RadixIPLookup;

class PoundRadixIPLookup : public Element { public:

    PoundRadixIPLookup();
    ~PoundRadixIPLookup();

    const char *class_name() const	{ return "PoundRadixIPLookup"; }

    int configure(Vector<String> &conf, ErrorHandler *errh);
    int initialize(ErrorHandler *errh);

    bool run_task(Task *task);

  private:

    RadixIPLookup *_l;
    Task _task;

};

CLICK_ENDDECLS
#endif
