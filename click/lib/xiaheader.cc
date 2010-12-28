// -*- related-file-name: "../include/click/xiaheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
#include <click/confparse.hh>
#include <click/xid.hh>
#include <click/standard/xiaxidinfo.hh>
#include <click/straccum.hh>
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

bool
XIAPath::parse_dag(const String& s, Element* context)
{
    _node_list.clear();
#ifndef CLICK_TOOL
    return cp_xid_dag(s, &_node_list, context);
#else
    return cp_xid_dag(s, &_node_list);
#endif
}

bool
XIAPath::parse_re(const String& s, Element* context)
{
    _node_list.clear();
#ifndef CLICK_TOOL
    return cp_xid_re(s, &_node_list, context);
#else
    return cp_xid_re(s, &_node_list);
#endif
}

void
XIAPath::parse_node(const struct click_xia_xid_node* node, size_t n)
{
    _node_list.clear();
    for (size_t i = 0; i < n; i++)
        _node_list.push_back(node[i]);
}

String
XIAPath::unparse_dag(Element* context)
{
    StringAccum sa;

    for (int i = -1; i < _node_list.size(); i++) {
        size_t wrapped_i;
        if (i < 0)
            wrapped_i = _node_list.size() + i;
        else
            wrapped_i = i;

        const struct click_xia_xid_node& node = _node_list[wrapped_i];

        if (i >= 0) {
            String name = XIAXIDInfo::revquery_xid(&node.xid, context);
            if (name.length() != 0)
                sa << name;
            else
                sa << XID(node.xid);
            sa << ' ';
        }

        if (i < _node_list.size() - 1) {
            for (size_t j = 0; j < CLICK_XIA_XID_EDGE_NUM; j++)
            {
                if (node.edge[j].idx == CLICK_XIA_XID_EDGE_UNUSED)
                    continue;
                sa << (int)node.edge[j].idx << ' ';
                //if (node.edge[j].visited)
                //    sa << '*';
            }
        }
    }
    return String(sa.data(), sa.length() - 1 /* exclusing the last space character */);
}

String
XIAPath::unparse_re(Element* context)
{
    // TODO: implement
    assert(false);
    return "";
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


XIAPath
XIAHeader::dst_path() const
{
    XIAPath p;
    p.parse_node(_hdr->node, _hdr->dnode);
    return p;
}

XIAPath
XIAHeader::src_path() const
{
    XIAPath p;
    p.parse_node(_hdr->node + _hdr->dnode, _hdr->snode);
    return p;
}


XIAHeaderEncap::XIAHeaderEncap()
    : _hdr(NULL)
{
#ifndef NDEBUG
    static bool _check = false;
    if (!_check)
    {
        // do some alignment/packing test
        assert(sizeof(struct click_xia) == 8);
        assert(sizeof(struct click_xia_xid) == 21);
        assert(sizeof(struct click_xia_xid_edge) == 1);
        assert(sizeof(struct click_xia_xid_node) == 24);
        struct click_xia hdr;
        assert(reinterpret_cast<unsigned long>(&(hdr.node[0])) - reinterpret_cast<unsigned long>(&hdr) == 8);
        assert(reinterpret_cast<unsigned long>(&(hdr.node[1])) - reinterpret_cast<unsigned long>(&hdr) == 8 + 24);

        _check = true;
    }
#endif

    struct click_xia hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.ver = 1;
    hdr.last = static_cast<int8_t>(-1);
    hdr.hlim = static_cast<uint8_t>(250);
    copy_hdr(&hdr);
    assert(hdr_size() == sizeof(hdr));
}

XIAHeaderEncap::XIAHeaderEncap(const XIAHeaderEncap& r)
    : _hdr(NULL)
{
    copy_hdr(r._hdr);
    assert(hdr_size() == r.hdr_size());
}

XIAHeaderEncap::~XIAHeaderEncap()
{
    delete [] reinterpret_cast<uint8_t*>(_hdr);
    _hdr = NULL;
}

const struct click_xia*
XIAHeaderEncap::hdr() const
{
    return _hdr;
}

struct click_xia*
XIAHeaderEncap::hdr()
{
    return _hdr;
}

size_t
XIAHeaderEncap::hdr_size() const
{
    return XIAHeader::hdr_size(_hdr->dnode + _hdr->snode);
}

void
XIAHeaderEncap::set_nxt(uint8_t nxt)
{
    _hdr->nxt = nxt;
}

void
XIAHeaderEncap::set_plen(uint16_t plen)
{
    _hdr->plen = htons(plen);
}

void
XIAHeaderEncap::set_last(int8_t last)
{
    _hdr->last = last;
}

void
XIAHeaderEncap::set_hlim(uint8_t hlim)
{
    _hdr->hlim = hlim;
}

void
XIAHeaderEncap::set_dst_path(const XIAPath& path)
{
    _dst_path = path;
    update_hdr();
}

void
XIAHeaderEncap::set_src_path(const XIAPath& path)
{
    _src_path = path;
    update_hdr();
}

WritablePacket*
XIAHeaderEncap::encap(Packet* p_in, bool adjust_plen) const
{
    size_t header_len = hdr_size();
    size_t payload_len = p_in->length();

    WritablePacket* p = p_in->push(header_len);
    if (!p)
        return NULL;

    memcpy(p->data(), _hdr, header_len);  // copy the header
    if (adjust_plen)
        reinterpret_cast<struct click_xia*>(p->data())->plen = htons(payload_len);
    p->set_xia_header(reinterpret_cast<struct click_xia*>(p->data()), header_len);

    return p;
}

void
XIAHeaderEncap::copy_hdr(const struct click_xia* hdr)
{
    delete [] reinterpret_cast<uint8_t*>(_hdr);

    const size_t size = XIAHeader::hdr_size(hdr->dnode + hdr->snode);
    _hdr = reinterpret_cast<click_xia*>(new uint8_t[size]);
    memcpy(_hdr, hdr, size);

    _dst_path.parse_node(_hdr->node, hdr->dnode);
    _src_path.parse_node(_hdr->node + hdr->dnode, hdr->snode);
}

void
XIAHeaderEncap::update_hdr()
{
    size_t dnode = _dst_path.unparse_node_size();
    size_t snode = _src_path.unparse_node_size();
    size_t dsnode = dnode + snode;
    const size_t size = XIAHeader::hdr_size(dsnode);

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
