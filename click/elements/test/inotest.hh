#ifndef CLICK_INOTEST_HH
#define CLICK_INOTEST_HH
#include <click/element.hh>
CLICK_DECLS

/*
=c

InoTest()

=s test

runs regression tests for ClickIno

=d

InoTest runs ClickIno regression tests at initialization time. It does not route
packets.

*/

class InoTest : public Element { public:

    InoTest();
    ~InoTest();

    const char *class_name() const		{ return "InoTest"; }

    int initialize(ErrorHandler *errh);

};

CLICK_ENDDECLS
#endif
