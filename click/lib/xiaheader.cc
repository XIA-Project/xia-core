// -*- related-file-name: "../include/click/xiaheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

/** @file xiaheader.hh
 * @brief The Packet class models packets in Click.
 */

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
{
    const size_t size = XIAHeader::hdr_size(0);
    _hdr = reinterpret_cast<struct click_xia*>(new uint8_t[size]);
    memset(_hdr, 0, size);
    _hdr->ver = 1;
    _hdr->last = LAST_NODE_DEFAULT;
    _hdr->hlim = HLIM_DEFAULT;
    _hdr->nxt = CLICK_XIA_NXT_DATA;
    assert(hdr_size() == size);
}

XIAHeaderEncap::XIAHeaderEncap(const XIAHeaderEncap& r)
{
    const size_t size = r.hdr_size();
    _hdr = reinterpret_cast<struct click_xia*>(new uint8_t[size]);
    memcpy(_hdr, r._hdr, size);
    _dst_path = r._dst_path;
    _src_path = r._src_path;
    assert(hdr_size() == size);
}

XIAHeaderEncap::~XIAHeaderEncap()
{
    delete [] reinterpret_cast<uint8_t*>(_hdr);
    _hdr = NULL;
}

XIAHeaderEncap::XIAHeaderEncap(const XIAHeader& r)
{
    const size_t size = r.hdr_size();
    _hdr = reinterpret_cast<struct click_xia*>(new uint8_t[size]);
    memcpy(_hdr, r.hdr(), size);
    _dst_path = r.dst_path();
    _src_path = r.src_path();
    assert(hdr_size() == size);
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
    update();
}

void
XIAHeaderEncap::set_src_path(const XIAPath& path)
{
    _src_path = path;
    update();
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
    Timestamp now = Timestamp::now();
    p->timestamp_anno() = now;

    return p;
}

WritablePacket *
XIAHeaderEncap::encap_replace(Packet *p_in) const
{
	// New header size
	size_t header_len = hdr_size();

	// Old header size
	XIAHeader xiah(p_in->xia_header());
	size_t old_len = xiah.hdr_size();

	WritablePacket *p;

	// Resize packet to hold new header
	if(old_len < header_len) {
		p = p_in->push(header_len - old_len);
	} else {
		p_in->pull(old_len - header_len);
		p = p_in->push(0);
	}
	if(!p) {
		return NULL;
	}

	// Copy header into the packet and mark it as the network header
	memcpy(p->data(), _hdr, header_len);
	p->set_xia_header(reinterpret_cast<struct click_xia *>(p->data()),
			header_len);
	return p;
}

void
XIAHeaderEncap::update()
{
    size_t dnode = _dst_path.unparse_node_size();
    size_t snode = _src_path.unparse_node_size();
    size_t dsnode = dnode + snode;
    const size_t size = XIAHeader::hdr_size(dsnode);

    click_xia* new_hdr = reinterpret_cast<struct click_xia*>(new uint8_t[size]);

    // preserve the current header content except path information
    assert(size >= sizeof(struct click_xia));
    memcpy(new_hdr, _hdr, sizeof(struct click_xia));
    new_hdr->dnode = dnode;
    new_hdr->snode = snode;

    if (_dst_path.unparse_node(new_hdr->node, dnode) != dnode)
        assert(false);
    if (_src_path.unparse_node(new_hdr->node + dnode, snode) != snode)
        assert(false);

    delete [] reinterpret_cast<uint8_t*>(_hdr);
    _hdr = new_hdr;
}

CLICK_ENDDECLS
