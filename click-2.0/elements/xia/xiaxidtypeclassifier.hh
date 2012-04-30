#ifndef CLICK_XIAXIDTYPECLASSIFIER_HH
#define CLICK_XIAXIDTYPECLASSIFIER_HH
#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/vector.hh>
CLICK_DECLS

/*
=c
XIAXIDTypeClassifier(PATTERN_1, ..., PATTERN_N)

=s ip
classifies XIA packets by the type of source/destination XID

=d
Classifies XIA packets by the type of source/destination XID.
PATTERN is (src TYPE | dst TYPE | src_and_dst SRCTYPE DSTTYPE | src_or_dst SRCTYPE DSTTYPE | next NEXTTYPE | -).

=e

XIAXIDTypeClassifier(src AD, dst HID, -)
It outputs packets from AD to port 0, HID packets destined for HID to port 1, and other packets to port 2.

=a IPClassifier, IPFilter
*/

class XIAXIDTypeClassifier : public Element { public:

    XIAXIDTypeClassifier();
    ~XIAXIDTypeClassifier();

    const char *class_name() const		{ return "XIAXIDTypeClassifier"; }
    const char *port_count() const		{ return "1/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);

    void push(int port, Packet *);

protected:
    int match(Packet *);
    struct pattern 
    {
        enum { SRC = 0, DST, SRC_AND_DST, SRC_OR_DST, NEXT, ANY } type;
        uint32_t src_xid_type;
        uint32_t dst_xid_type;
        uint32_t next_xid_type;
    };

private:
    Vector<struct pattern> _patterns;
};

CLICK_ENDDECLS
#endif
