/*
 * xiaxidtypeclassifier.{cc,hh} -- simple XIA packet classifier
 */

#include <click/config.h>
#include "xiaxidtypeclassifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/xid.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

XIAXIDTypeClassifier::XIAXIDTypeClassifier()
{
}

XIAXIDTypeClassifier::~XIAXIDTypeClassifier()
{
}

int
XIAXIDTypeClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() != noutputs())
        return errh->error("need %d arguments, one per output port", noutputs());

    for (int i = 0; i < conf.size(); i++) {
        String str_copy = conf[i];
        String type_str = cp_shift_spacevec(str_copy);

        if (type_str == "-") {
            struct pattern pat;
            pat.type = pattern::ANY;
            pat.src_xid_type = pat.dst_xid_type = 0;

            _patterns.push_back(pat);
        }
        else if (type_str == "src" || type_str == "dst" || type_str == "next") {
            String xid_type_str = cp_shift_spacevec(str_copy);
            uint32_t xid_type;
            if (!cp_xid_type(xid_type_str, &xid_type))
                return errh->error("unrecognized XID type: ", xid_type_str.c_str());

            struct pattern pat;
            if (type_str == "src") {
                pat.type = pattern::SRC;
                pat.src_xid_type = xid_type;
                pat.dst_xid_type = 0;
                pat.next_xid_type = 0;
            }
            else if (type_str == "dst") {
                pat.type = pattern::DST;
                pat.src_xid_type = 0;
                pat.dst_xid_type = xid_type;
                pat.next_xid_type = 0;
            }
            else {
                pat.type = pattern::NEXT;
                pat.src_xid_type = 0;
                pat.dst_xid_type = 0;
                pat.next_xid_type = xid_type;
            }

            _patterns.push_back(pat);
        }
        else if (type_str == "src_and_dst" || type_str == "src_or_dst") {
            String xid_type_str = cp_shift_spacevec(str_copy);
            uint32_t xid_type0;
            if (!cp_xid_type(xid_type_str, &xid_type0))
                return errh->error("unrecognized XID type: ", xid_type_str.c_str());

            xid_type_str = cp_shift_spacevec(str_copy);
            uint32_t xid_type1;
            if (!cp_xid_type(xid_type_str, &xid_type1))
                return errh->error("unrecognized XID type: ", xid_type_str.c_str());

            struct pattern pat;
            if (type_str == "src_and_dst")
                pat.type = pattern::SRC_AND_DST;
            else
                pat.type = pattern::SRC_OR_DST;
            pat.src_xid_type = xid_type0;
            pat.dst_xid_type = xid_type1;
            pat.next_xid_type = 0;

            _patterns.push_back(pat);
        }
        else
            return errh->error("unrecognized pattern type: ", type_str.c_str());
    }
    return 0;
}

void
XIAXIDTypeClassifier::push(int, Packet *p)
{
    int port = match(p);
    if (port >= 0)
        output(port).push(p);
    else {
        // no match -- discard packet
        if (p)
            p->kill();
    }
}

int
XIAXIDTypeClassifier::match(Packet *p)
{
    const struct click_xia* hdr = p->xia_header();

    // commented out for microbenchmarks
    /*
    if (p==NULL) return -1;
    if (!hdr)
        return -1;
    if (hdr->dnode == 0 || hdr->snode == 0)
        return -1;
    */

    uint32_t dst_xid_type = hdr->node[hdr->dnode - 1].xid.type;
    uint32_t src_xid_type = hdr->node[hdr->dnode + hdr->snode - 1].xid.type;

    uint32_t next_xid_type = -1;

    {
        int last = hdr->last;
        if (last == LAST_NODE_DEFAULT)
            last = hdr->dnode - 1;
        const struct click_xia_xid_edge* edge = hdr->node[last].edge;
        if (XIA_NEXT_PATH_ANNO(p) < CLICK_XIA_XID_EDGE_NUM)
        {
            const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p)];
            if (current_edge.idx != CLICK_XIA_XID_EDGE_UNUSED)
                if (current_edge.idx < hdr->dnode)
                    next_xid_type = hdr->node[current_edge.idx].xid.type;
        }
    }

    // test all patterns
    // TODO: group patterns to avoid the switch jump
    for (int i = 0; i < _patterns.size(); i++) {
        const struct pattern& pat = _patterns[i];
        switch (pat.type) {
            case pattern::SRC:
                if (src_xid_type == pat.src_xid_type)
                    return i;
                break;
            case pattern::DST:
                if (dst_xid_type == pat.dst_xid_type)
                    return i;
                break;
            case pattern::SRC_AND_DST:
                if (src_xid_type == pat.src_xid_type && dst_xid_type == pat.dst_xid_type)
                    return i;
                break;
            case pattern::SRC_OR_DST:
                if (src_xid_type == pat.src_xid_type || dst_xid_type == pat.dst_xid_type)
                    return i;
                break;
            case pattern::NEXT:
                if (next_xid_type == pat.next_xid_type) {
                    return i;
                }
                break;
            case pattern::ANY:
                return i;
                break;
        }
    }
    return -1;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAXIDTypeClassifier)
ELEMENT_MT_SAFE(XIAXIDTypeClassifier)
