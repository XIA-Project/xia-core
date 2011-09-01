/*
 * xiaxidroutetable.{cc,hh} -- simple XID routing table
 */

#include <click/config.h>
#include "xiaxidroutetable.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/packet_anno.hh>
#if CLICK_USERLEVEL
#include <fstream>
#include <stdlib.h>
#endif
CLICK_DECLS

XIAXIDRouteTable::XIAXIDRouteTable()
{
}

XIAXIDRouteTable::~XIAXIDRouteTable()
{
}

int
XIAXIDRouteTable::configure(Vector<String> &conf, ErrorHandler *errh)
{
    click_chatter("XIAXIDRouteTable: configuring %s\n", this->name().c_str());

    _rem = -1;

    for (int i = 0; i < conf.size(); i++)
    {
        int ret = set_handler(conf[i], this, 0, errh);
        if (!ret)
            return ret;
    }

    return 0;
}

void
XIAXIDRouteTable::add_handlers()
{
    add_write_handler("add", set_handler, 0);
    add_write_handler("set", set_handler, (void*)1);
    add_write_handler("remove", remove_handler, 0);
#if CLICK_USERLEVEL
    add_write_handler("load", load_routes_handler, 0);
#endif
    add_write_handler("generate", generate_routes_handler, 0);
}

int
XIAXIDRouteTable::set_handler(const String &conf, Element *e, void *thunk, ErrorHandler *errh)
{
    XIAXIDRouteTable* table = static_cast<XIAXIDRouteTable*>(e);

    bool add_mode = !thunk;
    /*
    if (add_mode)
        click_chatter("XIAXIDRouteTable: adding %-10s to %s\n", conf.c_str(), e->name().c_str());
    else
        click_chatter("XIAXIDRouteTable: setting %-10s for %s\n", conf.c_str(), e->name().c_str());
    */

    String str_copy = conf;

    String xid_str = cp_shift_spacevec(str_copy);
    if (xid_str.length() == 0)
    {
        // ignore empty entry
        return 0;
    }

    int port;
    if (!cp_integer(str_copy, &port))
        return errh->error("invalid port: ", str_copy.c_str());
    if (port < 0 || port >= table->noutputs())
        return errh->error("port out of range: ", str_copy.c_str());

    if (xid_str == "-")
    {
        if (add_mode && table->_rem != -1)
            return errh->error("duplicate default route: ", xid_str.c_str());
        table->_rem = port;
    }
    else
    {
        XID xid;
        if (!cp_xid(xid_str, &xid, e))
            return errh->error("invalid XID: ", xid_str.c_str());
        if (add_mode && table->_rt.find(xid) != table->_rt.end())
            return errh->error("duplicate XID: ", xid_str.c_str());
        table->_rt[xid] = port;
    }
    return 0;
}

int
XIAXIDRouteTable::remove_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    XIAXIDRouteTable* table = static_cast<XIAXIDRouteTable*>(e);

    //click_chatter("XIAXIDRouteTable: removing %-10s from %s\n", conf.c_str(), e->name().c_str());

    String str_copy = conf;

    String xid_str = cp_shift_spacevec(str_copy);
    if (xid_str.length() == 0)
    {
        // ignore empty entry
        return 0;
    }

    int port;
    if (!cp_integer(str_copy, &port))
        return errh->error("invalid port: ", str_copy.c_str());
    if (port < 0 || port >= table->noutputs())
        return errh->error("port out of range: ", str_copy.c_str());

    if (xid_str == "-")
        table->_rem = -1;
    else
    {
        XID xid;
        if (!cp_xid(xid_str, &xid, e))
            return errh->error("invalid XID: ", xid_str.c_str());
        HashTable<XID, int>::iterator it = table->_rt.find(xid);
        if (it == table->_rt.end())
            return errh->error("nonexistent XID: ", xid_str.c_str());
        table->_rt.erase(it);
    }
    return 0;
}

#if CLICK_USERLEVEL
int
XIAXIDRouteTable::load_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
    std::ifstream in_f(conf.c_str());
    if (!in_f.is_open())
    {
        errh->error("could not open file: %s", conf.c_str());
        return -1;
    }

    int c = 0;
    while (!in_f.eof())
    {
        char buf[1024];
        in_f.getline(buf, sizeof(buf));

        if (strlen(buf) == 0)
            continue;

        if (set_handler(buf, e, 0, errh) != 0)
            return -1;

        c++;
    }
    click_chatter("loaded %d entries", c);

    return 0;
}
#endif

int
XIAXIDRouteTable::generate_routes_handler(const String &conf, Element *e, void *, ErrorHandler *errh)
{
#if CLICK_USERLEVEL
    XIAXIDRouteTable* table = dynamic_cast<XIAXIDRouteTable*>(e);
#else
    XIAXIDRouteTable* table = reinterpret_cast<XIAXIDRouteTable*>(e);
#endif
    assert(table);

    String conf_copy = conf;

    String xid_type_str = cp_shift_spacevec(conf_copy);
    uint32_t xid_type;
    if (!cp_xid_type(xid_type_str, &xid_type))
        return errh->error("invalid XID type: ", xid_type_str.c_str());

    String count_str = cp_shift_spacevec(conf_copy);
    int count;
    if (!cp_integer(count_str, &count))
        return errh->error("invalid entry count: ", count_str.c_str());

    String port_str = cp_shift_spacevec(conf_copy);
    int port;
    if (!cp_integer(port_str, &port))
        return errh->error("invalid port: ", port_str.c_str());

#if CLICK_USERLEVEL
    unsigned short xsubi[3];
    xsubi[0] = 1;
    xsubi[1] = 2;
    xsubi[2] = 3;
#else
    struct rnd_state state;
    state.s1= 1;
    state.s2= 2;
    state.s3= 3;
#endif

    struct click_xia_xid xid_d;
    xid_d.type = xid_type;

    for (int i = 0; i < count; i++)
    {
        uint8_t* xid = xid_d.id;
        const uint8_t* xid_end = xid + CLICK_XIA_XID_ID_LEN;

        while (xid != xid_end)
        {
#if CLICK_USERLEVEL
            *reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(nrand48(xsubi));
#else
            *reinterpret_cast<uint32_t*>(xid) = static_cast<uint32_t>(prandom32(&state));
#endif
            xid += sizeof(uint32_t);
        }

        table->_rt[XID(xid_d)] = port;
    }

    click_chatter("generated %d entries", count);
    return 0;
}

void
XIAXIDRouteTable::push(int, Packet *p)
{
    int port = lookup_route(p);
    if (port >= 0)
        output(port).push(p);
    else
    {
        // no match -- discard packet
        p->kill();
    }
}

int
XIAXIDRouteTable::lookup_route(Packet *p)
{
    const struct click_xia* hdr = p->xia_header();
    /*
    if (!hdr)
        return -1;
    if (hdr->last >= (int)hdr->dnode)
    {
        click_chatter("invalid last pointer (out of range)\n");
        return -1;
    }

    if (hdr->last == (int)hdr->dnode - 1)
    {
        click_chatter("invalid last pointer (already arrived at dest)\n");
        return -1;
    }
    */

    int last = hdr->last;
    if (last < 0)
        last += hdr->dnode;
    const struct click_xia_xid_edge* edge = hdr->node[last].edge;
    const struct click_xia_xid_edge& current_edge = edge[XIA_NEXT_PATH_ANNO(p)];
    const int& idx = current_edge.idx;
    if (idx == CLICK_XIA_XID_EDGE_UNUSED)
    {
        // unused edge -- use default route
        return _rem;
    }
    /*
    if (idx <= hdr->last)
    {
        // The DAG representaion prohibits non-increasing index
        click_chatter("invalid idx field: %d (not larger than last field %d)", idx, last);
        return -1;
    }
    */
    const struct click_xia_xid_node& node = hdr->node[idx];
    HashTable<XID, int>::const_iterator it = _rt.find(node.xid);
    if (it != _rt.end())
    {
        //click_chatter("%s %s %d\n", (*it).first.unparse().c_str(), XID(node.xid).unparse().c_str(), (*it).second);
        return (*it).second;
    }
    else
    {
        // no match -- use default route
        return _rem;
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAXIDRouteTable)
ELEMENT_MT_SAFE(XIAXIDRouteTable)
