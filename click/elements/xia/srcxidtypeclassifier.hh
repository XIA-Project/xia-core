#ifndef CLICK_SRCXIDTYPECLASSIFIER_HH
#define CLICK_SRCXIDTYPECLASSIFIER_HH
#include <click/element.hh>
#include <click/hashtable.hh>
#include <clicknet/xia.h>
CLICK_DECLS

/*
=c
SRCXIDTypeClassifier(PATTERN_1, ..., PATTERN_N)

=s ip
classifies XIA packets by the type of source XID

=d
Classifies XIA packets by the type of source XID.

=e

SRCXIDTypeClassifier(AD, HID, -)
outputs AD packets to port 0, HID packets to port 1, other packets to port 2.

=a IPClassifier, IPFilter
*/

class SrcXIDTypeClassifier : public Element { public:

    SrcXIDTypeClassifier();
    ~SrcXIDTypeClassifier();

    const char *class_name() const		{ return "SrcXIDTypeClassifier"; }
    const char *port_count() const		{ return "1/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);

    void push(int port, Packet *);

protected:
    int parse_xid_type(const String& str);
    int match(Packet *);

private:
    int _map[CLICK_XIA_XID_TYPE_MAX + 1];
    int _rem;
};

CLICK_ENDDECLS
#endif
