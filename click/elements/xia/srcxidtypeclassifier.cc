/*
 * srcxidtypeclassifier.{cc,hh} -- simple XIA packet classifier
 */

#include <click/config.h>
#include "srcxidtypeclassifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/xid.hh>
CLICK_DECLS

SrcXIDTypeClassifier::SrcXIDTypeClassifier()
{
}

SrcXIDTypeClassifier::~SrcXIDTypeClassifier()
{
}

int
SrcXIDTypeClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (conf.size() != noutputs())
	return errh->error("need %d arguments, one per output port", noutputs());

    for (size_t i = 0; i < CLICK_XIA_XID_TYPE_MAX + 1; i++)
        _map[i] = -1;
    _rem = -1;

    for (int i = 0; i < conf.size(); i++)
    {
        String str_copy = conf[i];
        String pattern = cp_shift_spacevec(str_copy);
        if (pattern == "-")
        {
            if (_rem != -1)
                return errh->error("duplicate pattern: ", pattern.c_str());
            _rem = i;
        }
        else
        {
            int xid_type;
            if (!cp_xid_type(pattern, &xid_type))
                return errh->error("unrecognized XID type: ", pattern.c_str());
            if (xid_type >= CLICK_XIA_XID_TYPE_MAX + 1)
                return errh->error("invalid XID type: ", pattern.c_str());
            if (_map[xid_type] != -1)
                return errh->error("duplicate pattern: ", pattern.c_str());
            _map[xid_type] = i;
        }
    }
    return 0;
}

void
SrcXIDTypeClassifier::push(int, Packet *p)
{
    int port = match(p);
    if (port >= 0)
        checked_output_push(port, p);
    else
    {
        // no match -- discard packet
        p->kill();
    }
}

int
SrcXIDTypeClassifier::match(Packet *p)
{
    const struct click_xia* hdr = p->xia_header();
    if (!hdr)
        return -1;
    int xid_type = hdr->node[hdr->dnode - 1].xid.type;
    //printf("%s\n", XID(hdr->node[hdr->dnode - 1].xid).unparse().c_str());
    //printf("%d %d\n", hdr->dnode, xid_type);
    if (xid_type < 0 || xid_type >= CLICK_XIA_XID_TYPE_MAX + 1)
        return -1;
    int port = _map[xid_type];
    if (port < 0)
        port = _rem;
    return port;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(SrcXIDTypeClassifier)
ELEMENT_MT_SAFE(SrcXIDTypeClassifier)
