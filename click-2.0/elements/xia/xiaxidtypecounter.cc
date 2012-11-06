/*
 * xiaxidtypecounter.{cc,hh} -- simple XIA packet classifying counter
 */

#include <click/config.h>
#include "xiaxidtypecounter.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/handlercall.hh>
#include <click/confparse.hh>
#include <click/xid.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

XIAXIDTypeCounter::XIAXIDTypeCounter() : _stats(0), _size(0)
{
}

XIAXIDTypeCounter::~XIAXIDTypeCounter()
{
    if (_stats) delete[] _stats;
}

void
XIAXIDTypeCounter::cleanup(CleanupStage)
{
    if (_stats) {
    	delete[] _stats;
		_stats = NULL;
    }
}

String
XIAXIDTypeCounter::count_str()
{
    String str;
    for (int i = 0; i < _size; i++)
        str += "port " + String(i) + " " + String(_stats[i]) + "\n";
    return str;
}

enum {XIDCOUNT};

String
XIAXIDTypeCounter::read_handler(Element *e, void *thunk)
{
    switch ((intptr_t)thunk) {
		case XIDCOUNT:
			return dynamic_cast<XIAXIDTypeCounter*>(e)->count_str();

		default:
			return "<error>";
    }
}

void
XIAXIDTypeCounter::add_handlers()
{
    add_read_handler("count", read_handler, (void *)XIDCOUNT);
}

int
XIAXIDTypeCounter::configure(Vector<String> &conf, ErrorHandler *errh)
{
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

    _size = _patterns.size();
    _stats = new uint32_t[_size];
    for (int i = 0; i < _size; i++)
			_stats[i]=0;
    return 0;
}

void 
XIAXIDTypeCounter::count_stats(int cl)
{
    assert(cl < _size);
    _stats[cl]++;
}

Packet *
XIAXIDTypeCounter::simple_action(Packet *p)
{
    int classification = match(p);
    if (classification >= 0)
		count_stats(classification);
    return p;
}


int
XIAXIDTypeCounter::match(Packet *p)
{
    const struct click_xia* hdr = p->xia_header();

    /*
    if (p==NULL) return -1;
    if (!hdr) 
        return -1;
    if (hdr->dnode == 0 || hdr->snode == 0)
        return -1;
    */

    struct click_xia_xid __dstID =  hdr->node[hdr->dnode - 1].xid;
    uint32_t dst_xid_type = ntohl(__dstID.type);
    struct click_xia_xid __srcID = hdr->node[hdr->dnode + hdr->snode - 1].xid;
    uint32_t src_xid_type = ntohl(__srcID.type);
#if 0
    // hack to the hack. Filter out weird xid types so the following code doesn't smash the stack
    // for some reason we get garbage data periodically that causes the XID constructor to fail
    if (src_xid_type > CLICK_XIA_XID_TYPE_IP || dst_xid_type > CLICK_XIA_XID_TYPE_IP)
	return -1;

//    printf ("%08x %08x\n", ntohl(__dstID.type), ntohl(__srcID.type));

    XID dstXID(__dstID);
    XID srcXID(__srcID);

    const char *ss = srcXID.unparse().c_str();
    const char *ds = dstXID.unparse().c_str();

    // Hack: filtering out daemon traffic (e.g., xroute, xhcp, name-server)
    if (strcmp(ss, "SID:1110000000000000000000000000000000001111") == 0 ||
        strcmp(ss, "SID:1110000000000000000000000000000000001112") == 0 ||
    	strcmp(ss, "SID:1110000000000000000000000000000000001113") == 0 ||
    	strcmp(ds, "SID:1110000000000000000000000000000000001111") == 0 ||
    	strcmp(ds, "SID:1110000000000000000000000000000000001112") == 0 ||
    	strcmp(ds, "SID:1110000000000000000000000000000000001113") == 0 ||
    	strcmp(ds, "HID:1111111111111111111111111111111111111111") == 0) {
    	return -1;
    }
#endif
    uint32_t next_xid_type = -1;
    {
        int last = hdr->last;
        if (last < 0)
            last += hdr->dnode;
        const struct click_xia_xid_edge* edge = hdr->node[last].edge;
        if (XIA_NEXT_PATH_ANNO(p) < CLICK_XIA_XID_EDGE_NUM) {
            const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p)];
            if (current_edge.idx != CLICK_XIA_XID_EDGE_UNUSED)
                if (current_edge.idx < hdr->dnode)
                    next_xid_type = hdr->node[current_edge.idx].xid.type;
        }
    }   

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
                if (next_xid_type == pat.next_xid_type)
                    return i;
                break;
            case pattern::ANY:
                return i;
                break;
        }
    }

    return -1;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAXIDTypeCounter)
ELEMENT_MT_SAFE(XIAXIDTypeCounter)
