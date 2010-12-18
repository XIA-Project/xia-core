#ifndef CLICK_XIDTYPECLASSIFIER_HH
#define CLICK_XIDTYPECLASSIFIER_HH
#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/vector.hh>
CLICK_DECLS

/*
=c
XIDTypeClassifier(PATTERN_1, ..., PATTERN_N)

=s ip
classifies XIA packets by the type of source/destination XID

=d
Classifies XIA packets by the type of source/destination XID.
PATTERN is (src TYPE | dst TYPE | src_and_dst SRCTYPE DSTTYPE | src_or_dst SRCTYPE DSTTYPE | -).

=e

XIDTypeClassifier(src AD, dst HID, -)
It outputs packets from AD to port 0, HID packets towards HID to port 1, and other packets to port 2.

=a IPClassifier, IPFilter
*/

class XIDTypeClassifier : public Element { public:

    XIDTypeClassifier();
    ~XIDTypeClassifier();

    const char *class_name() const		{ return "XIDTypeClassifier"; }
    const char *port_count() const		{ return "1/-"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);

    void push(int port, Packet *);

protected:
    int match(Packet *);
    struct pattern 
    {
        enum { SRC = 0, DST, SRC_AND_DST, SRC_OR_DST, ANY } type;
        uint8_t src_xid_type;
        uint8_t dst_xid_type;
    };

private:
    Vector<struct pattern> _patterns;
};

CLICK_ENDDECLS
#endif
