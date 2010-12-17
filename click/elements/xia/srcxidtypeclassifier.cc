/*
 * srcxidtypeclassifier.{cc,hh} -- simple XIA packet classifier
 */

#include <click/config.h>
#include "srcxidtypeclassifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
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
            if (_rem == -1)
                _rem = i;
            else
                return errh->error("duplicate pattern: ", pattern.c_str());
        }
        else
        {
            int xid_type = parse_xid_type(pattern);
            if (xid_type >= 0)
            {
                if (xid_type >= CLICK_XIA_XID_TYPE_MAX + 1)
                    return errh->error("invalid XID type: ", pattern.c_str());
                if (_map[xid_type] != -1)
                    return errh->error("duplicate pattern: ", pattern.c_str());
                _map[xid_type] = i;
            }
            else
                return errh->error("unrecognized XID type: ", pattern.c_str());
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
    }
}

int
SrcXIDTypeClassifier::parse_xid_type(const String& str)
{
    // TODO: remove duplicate code with cp_xid
    if (str.compare(String("UNDEF")) == 0)
       return CLICK_XIA_XID_TYPE_UNDEF;
    else if (str.compare(String("AD")) == 0)
       return CLICK_XIA_XID_TYPE_AD;
    else if (str.compare(String("CID")) == 0)
       return CLICK_XIA_XID_TYPE_CID;
    else if (str.compare(String("HID")) == 0)
       return CLICK_XIA_XID_TYPE_HID;
    else if (str.compare(String("SID")) == 0)
       return CLICK_XIA_XID_TYPE_SID;
    else
        return -1;
}

int
SrcXIDTypeClassifier::match(Packet *p)
{
    const struct click_xia* hdr = p->xia_header();
    if (!hdr)
        return -1;
    if (hdr->dnode + hdr->dint >= hdr->dsnode)
        return -1;
    int xid_type = hdr->node[hdr->dnode + hdr->dint].xid.type;
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
