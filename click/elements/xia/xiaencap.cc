/*
 * xiancap.{cc,hh} -- element encapsulates packet in XIA header
 * Dongsu Han 
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "xiaencap.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XIAEncap::XIAEncap()
    : _xiah(NULL)
{
    _xiah = new XIAHeader(0);
}

XIAEncap::~XIAEncap()
{
    delete _xiah;
}

int
XIAEncap::configure(Vector<String> &conf, ErrorHandler *errh)
{
    String dst_str;
    String src_str;
    int nxt = -1;
    uint8_t hlim = 250;
    uint8_t last = 0;

    if (cp_va_kparse(conf, this, errh,
                   "NXT", cpkP+cpkM, cpNamedInteger, NameInfo::T_IP_PROTO, &nxt,
                   "SRC", cpkP+cpkM, cpArgument, &src_str,
                   "DST", cpkP+cpkM, cpArgument, &dst_str,
                   "LAST", 0, cpByte, &last,
                   "HLIM", 0, cpByte, &hlim,
                   cpEnd) < 0)
        return -1;

    if (nxt < 0 || nxt > 255)
        return errh->error("bad next protocol");

    XIDNodeList dst_nodes;
    XIDNodeList src_nodes;

    String dst_type = cp_shift_spacevec(dst_str);
    if (dst_type == "DAG")
        cp_xid_dag(dst_str, &dst_nodes);
    else if (dst_type == "RE")
        cp_xid_re(dst_str, &dst_nodes);
    else
        return errh->error("unrecognized dst type: %s", dst_type.c_str());

    String src_type = cp_shift_spacevec(src_str);
    if (src_type == "DAG")
        cp_xid_dag(src_str, &src_nodes);
    else if (src_type == "RE")
        cp_xid_re(src_str, &src_nodes);
    else
        return errh->error("unrecognized src type: %s", src_type.c_str());

    delete _xiah;   // this is safe even when _xiah == NULL

    size_t nxids = dst_nodes.size() + src_nodes.size();
    _xiah = new XIAHeader(nxids);
    if (!_xiah)
        return errh->error("failed to allocate");

    _xiah->hdr().ver = 1;
    _xiah->hdr().nxt = nxt;
    _xiah->hdr().nxids = nxids;
    _xiah->hdr().ndst = dst_nodes.size();
    _xiah->hdr().last = last;
    _xiah->hdr().hlim = hlim;
    _xiah->hdr().flags = 0;
    _xiah->hdr().plen = _xiah->size();

    size_t node_idx = 0;
    for (int i = 0; i < dst_nodes.size(); i++)
        _xiah->hdr().node[node_idx++] = dst_nodes[i];
    for (int i = 0; i < src_nodes.size(); i++)
        _xiah->hdr().node[node_idx++] = src_nodes[i];

    return 0;
}


int
XIAEncap::initialize(ErrorHandler *)
{
  return 0;
}


Packet *
XIAEncap::simple_action(Packet *p_in)
{
    int header_len = _xiah->size();
    int payload_len = p_in->length();
    WritablePacket *p = p_in->push(header_len); // make room for XIA header
    if (!p) return 0;

    click_xia *xiah = reinterpret_cast<click_xia *>(p->data());
    memcpy(xiah, &_xiah->hdr(), header_len); // copy the header
    xiah->plen = htons(payload_len);

    p->set_xia_header(xiah, header_len); // set the network header position (nh)
    return p;
}

bool cp_xid_dag(const String& str, XIDNodeList* result)
{
    String str_copy = str;
    while (true)
    {
        click_xia_xid_node node;
        memset(&node, 0, sizeof(node));

        String xid_str = cp_shift_spacevec(str_copy);
        if (xid_str.length() == 0)
            break;

        // parse XID
        if (!cp_xid(xid_str, &node.xid))
        {
            click_chatter("unrecognized XID format: %s", str.c_str());
            return false;
        }

        // parse increment
        int incr;
        if (!cp_integer(cp_shift_spacevec(str_copy), &incr))
        {
            click_chatter("unrecognized increment: %s", str.c_str());
            return false;
        }
        if (incr < 0 || incr > 255)
        {
            click_chatter("increment out of range: %s", str.c_str());
            return false;
        }
        node.incr = incr;

        result->push_back(node);
    }
    return true;
}

bool
cp_xid_re(const String& str, XIDNodeList* result)
{
    String str_copy = str;

    click_xia_xid prev_xid;
    click_xia_xid_node node;

    // parse the first XID
    if (!cp_xid(cp_shift_spacevec(str_copy), &prev_xid))
    {
        click_chatter("unrecognized XID format: %s", str.c_str());
        return false;
    }

    // parse iterations: ( ("(" fallback path ")")? main node )*
    while (true)
    {
        String head = cp_shift_spacevec(str_copy);
        if (head.length() == 0)
            break;

        Vector<struct click_xia_xid> fallback;
        if (head == "(")
        {
            // parse fallback path for the next main node
            while (true)
            {
                String tail = cp_shift_spacevec(str_copy);
                if (tail == ")")
                    break;

                click_xia_xid xid;
                cp_xid(tail, &xid);
                fallback.push_back(xid);
            }
        }

        // parse the next main node
        click_xia_xid next_xid;
        cp_xid(cp_shift_spacevec(str_copy), &next_xid);

        // link the prev main node to the next main node
        // 1 + 2*|fallback path| nodes before next main node
        int dist_to_next_main = 1 + 2 * fallback.size();

        node.xid = prev_xid;
        node.incr = dist_to_next_main;
        result->push_back(node);
        dist_to_next_main--;

        if (fallback.size() > 0)
        {
            node.xid = prev_xid;
            node.incr = 1;
            result->push_back(node);
            dist_to_next_main--;

            for (int i = 0; i < fallback.size(); i++)
            {
                // link to the next main node (implicit link)
                node.xid = fallback[i];
                node.incr = dist_to_next_main;
                result->push_back(node);
                dist_to_next_main--;

                if (i != fallback.size() - 1)
                {
                    // link to the next fallback node
                    node.xid = fallback[i];
                    node.incr = 1;
                    result->push_back(node);
                    dist_to_next_main--;
                }
            }
        }

        assert(dist_to_next_main == 0);

        prev_xid = next_xid;
    }

    // push the destination node
    node.xid = prev_xid;
    node.incr = 0;          // not meaningful
    result->push_back(node);

    return true;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAEncap)
ELEMENT_MT_SAFE(XIAEncap)
