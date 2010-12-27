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

XIAPath::XIAPath()
{
}

XIAPath::XIAPath(const XIAPath& r)
{
    _node_list = r._node_list;
}

XIAPath&
XIAPath::operator=(const XIAPath& r)
{
    _node_list = r._node_list;
    return *this;
}

void
XIAPath::parse_dag(const String& s, Element* context)
{
    _node_list.clear();
#ifndef CLICK_TOOL
    cp_xid_dag(s, &_node_list, context);
#else
    cp_xid_dag(s, &_node_list);
#endif
}

void
XIAPath::parse_re(const String& s, Element* context)
{
    _node_list.clear();
#ifndef CLICK_TOOL
    cp_xid_re(s, &_node_list, context);
#else
    cp_xid_re(s, &_node_list);
#endif
}

void
XIAPath::parse_node(const struct click_xia_xid_node* node, size_t n)
{
    _node_list.clear();
    for (size_t i = 0; i < n; i++)
        _node_list.push_back(node[i]);
}

size_t
XIAPath::unparse_node_size() const
{
    return _node_list.size();
}

size_t
XIAPath::unparse_node(struct click_xia_xid_node* node, size_t n) const
{
    if (n > unparse_node_size())
        n = unparse_node_size();
    for (size_t i = 0; i < n; i++)
        node[i] = _node_list[i];
    return n;
}

XIAHeader::XIAHeader(size_t dsnode)
{
    const size_t size = XIAHeader::size(dsnode);
    _hdr = reinterpret_cast<click_xia*>(new uint8_t[size]);

    memset(_hdr, 0, size);
    _hdr->ver = 1;
    _hdr->last = static_cast<int8_t>(-1);
    _hdr->hlim = static_cast<uint8_t>(250);
}

XIAHeader::XIAHeader(const XIAHeader& r)
{
    _hdr = NULL;
    set_hdr(r._hdr);
}

XIAHeader::XIAHeader(const struct click_xia& hdr)
{
    _hdr = NULL;
    set_hdr(&hdr);
}

XIAHeader::XIAHeader(const Packet& p)
{
    _hdr = NULL;
    set_hdr(p.xia_header());
}

XIAHeader::~XIAHeader()
{
    delete [] _hdr;
    _hdr = NULL;
}

XIAHeader&
XIAHeader::operator=(const XIAHeader& r)
{
    set_hdr(r._hdr);
    return *this;
}

void
XIAHeader::set_hdr(const struct click_xia* hdr)
{
    delete [] reinterpret_cast<uint8_t*>(_hdr);

    const size_t size = XIAHeader::size(hdr->dnode + hdr->snode);
    _hdr = reinterpret_cast<click_xia*>(new uint8_t[size]);
    memcpy(_hdr, hdr, size);

    _dst_path.parse_node(_hdr->node, hdr->dnode);
    _src_path.parse_node(_hdr->node + hdr->dnode, hdr->snode);
}

void
XIAHeader::set_dst_path(const XIAPath& path)
{
    _dst_path = path;
    update_hdr();
}

void
XIAHeader::set_src_path(const XIAPath& path)
{
    _src_path = path;
    update_hdr();
}

void
XIAHeader::update_hdr()
{
    size_t dnode = _dst_path.unparse_node_size();
    size_t snode = _src_path.unparse_node_size();
    size_t dsnode = dnode + snode;
    const size_t size = XIAHeader::size(dsnode);

    click_xia* new_hdr = reinterpret_cast<click_xia*>(new uint8_t[size]);

    // preserve the current header content except path information
    memcpy(new_hdr, _hdr, sizeof(struct click_xia));
    new_hdr->dnode = dnode;
    new_hdr->snode = snode;

    if (_dst_path.unparse_node(new_hdr->node, dnode) != dnode)
        assert(false);
    if (_src_path.unparse_node(new_hdr->node + dnode, snode) != snode)
        assert(false);

    delete _hdr;
    _hdr = new_hdr;
}

CLICK_ENDDECLS
