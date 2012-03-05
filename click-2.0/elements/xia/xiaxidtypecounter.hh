#ifndef CLICK_XIAXIDTYPECOUNTER_HH
#define CLICK_XIAXIDTYPECOUNTER_HH
#include <click/element.hh>
#include <clicknet/xia.h>
#include <click/vector.hh>
CLICK_DECLS

/*
=c
XIAXIDTypeCounter(PATTERN_1, ..., PATTERN_N)

=s ip
counts XIA packets by the type of source/destination XID

=d
Counts XIA packets by the type of source/destination XID.
PATTERN is (src TYPE | dst TYPE | src_and_dst SRCTYPE DSTTYPE | src_or_dst SRCTYPE DSTTYPE | next NEXTTYPE | -).

=e

XIAXIDTypeCounter(src AD, dst HID, -)

=a 
*/

class XIAXIDTypeCounter : public Element { public:

    XIAXIDTypeCounter();
    ~XIAXIDTypeCounter();

    const char *class_name() const		{ return "XIAXIDTypeCounter"; }
    const char *port_count() const		{ return "1/1"; }
    const char *processing() const		{ return PROCESSING_A_AH; }

    int configure(Vector<String> &, ErrorHandler *);

    Packet *simple_action(Packet *);
    void cleanup(CleanupStage);
    void add_handlers();

protected:
    int match(Packet *);
    String count_str();
    struct pattern 
    {
        enum { SRC = 0, DST, SRC_AND_DST, SRC_OR_DST, NEXT, ANY } type;
        uint32_t src_xid_type;
        uint32_t dst_xid_type;
        uint32_t next_xid_type;
    };

private:
    void count_stats(int cl);
    static String read_handler(Element *e, void *thunk);

    Vector<struct pattern> _patterns;
    uint32_t* _stats;
    int _size;
};

CLICK_ENDDECLS
#endif
