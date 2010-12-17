/*
 * staticxidlookup.{cc,hh} -- simple static XID routing table
 */

#include <click/config.h>
#include "staticxidlookup.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

StaticXIDLookup::StaticXIDLookup()
{
}

StaticXIDLookup::~StaticXIDLookup()
{
}

int
StaticXIDLookup::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _rem = -1;
    _dest = -1;

    for (int i = 0; i < conf.size(); i++)
    {
        String str_copy = conf[i];

        String xid_str = cp_shift_spacevec(str_copy);
        if (xid_str.length() == 0)
        {
            // allow empty entry
            continue;
        }

        int out;
        if (!cp_integer(str_copy, &out))
            return errh->error("invalid output: ", str_copy.c_str());
        if (out < 0 || out >= noutputs())
            return errh->error("output out of range: ", str_copy.c_str());

        if (xid_str == "-")
        {
            if (_rem != -1)
                return errh->error("duplicate default route: ", xid_str.c_str());
            _rem = out;
        }
        else if (xid_str == "dest")
        {
            if (_dest != -1)
                return errh->error("duplicate destination route: ", xid_str.c_str());
            _dest = out;
        }
        else
        {
            XID xid;
            if (!cp_xid(xid_str, &xid, this))
                return errh->error("invalid XID: ", xid_str.c_str());
            if (_rt.find(xid) != _rt.end())
                return errh->error("duplicate XID: ", xid_str.c_str());
            _rt[xid] = out;
        }
    }
    return 0;
}

void
StaticXIDLookup::push(int, Packet *p)
{
    int port = lookup(p);
    if (port >= 0)
        checked_output_push(port, p);
    else
    {
        // no match -- discard packet
        p->kill();
    }
}

int
StaticXIDLookup::lookup(Packet *p)
{
    const struct click_xia* hdr = p->xia_header();
    if (!hdr)
        return -1;
    if (hdr->last >= (int)hdr->dnode)
        return -1;

    if (hdr->last >= (int)hdr->dint)
    {
        // the packet has arrived at the destination.
        // send the packet to userlevel
        return _dest;
    }

    int port = _rem;

    int last = hdr->last;
    if (last < 0)
        last += hdr->dnode;
    const struct click_xia_xid_edge* edge = hdr->node[last].edge;
    for (int i = 0; i < 4; i++)
    {
        const struct click_xia_xid_edge& current_edge = edge[i];
        const int& idx = current_edge.idx;
        if (idx == CLICK_XIA_XID_EDGE_UNUSED)
            continue;
        const struct click_xia_xid_node& node = hdr->node[idx];
        HashTable<XID, int>::const_iterator it = _rt.find(node.xid);
        if (it != _rt.end())
        {
            port = (*it).second;
            //printf("%s %s %d\n", (*it).first.unparse().c_str(), XID(node.xid).unparse().c_str(), port);
            SET_PAINT_ANNO(p, i);   // annotation for XIANextHop
            break;
        }
    }

    return port;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StaticXIDLookup)
ELEMENT_MT_SAFE(StaticXIDLookup)
