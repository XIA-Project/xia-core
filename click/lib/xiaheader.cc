// -*- related-file-name: "../include/click/xiaheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
#include <click/confparse.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

/** @file xiaheader.hh
 * @brief The Packet class models packets in Click.
 */

XIAHeader::XIAHeader(size_t nxids)
{
    const size_t size = XIAHeader::size(nxids);
    _hdr = reinterpret_cast<click_xia*>(new uint8_t[size]);
    memset(_hdr, 0, size);
}

XIAHeader::XIAHeader(const struct click_xia& hdr)
{
    const size_t size = XIAHeader::size(hdr.nxids);
    _hdr = reinterpret_cast<click_xia*>(new uint8_t[size]);
    memcpy(_hdr, &hdr, size);
}

XIAHeader::~XIAHeader()
{
    delete [] _hdr;
    _hdr = NULL;
}


bool
cp_xid_dag(const String& str, Vector<struct click_xia_xid_node>* result)
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
cp_xid_re(const String& str, Vector<struct click_xia_xid_node>* result)
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
