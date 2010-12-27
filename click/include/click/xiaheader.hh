// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaheader.cc" -*-
#ifndef CLICK_XIAHEADER_HH
#define CLICK_XIAHEADER_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/vector.hh>

CLICK_DECLS
class StringAccum;
class Element;

class XIAPath { public:
    XIAPath();
    XIAPath(const XIAPath& r);

    XIAPath& operator=(const XIAPath& r);

    void parse_dag(const String& s, Element* context = NULL);
    void parse_re(const String& s, Element* context = NULL);
    void parse_node(const struct click_xia_xid_node* node, size_t n);

    size_t unparse_node_size() const;
    size_t unparse_node(struct click_xia_xid_node* node, size_t n) const;

private:
    Vector<struct click_xia_xid_node> _node_list;
};

class XIAHeader { public:

    /** @brief Construct an XIAHeader */
    XIAHeader(size_t dsnode = 0);
    XIAHeader(const XIAHeader& r);
    XIAHeader(const struct click_xia& hdr);
    XIAHeader(const Packet& p);     // Packet& is used to avoid ambiguity
    ~XIAHeader();

    XIAHeader& operator=(const XIAHeader& r);

    static size_t size(uint8_t dsnode);

    const struct click_xia& hdr() const;
    struct click_xia& hdr();
    operator struct click_xia() const;

    void set_hdr(const struct click_xia* hdr);

    size_t size() const;

    void set_dst_path(const XIAPath& path);
    void set_src_path(const XIAPath& path);

    inline const XIAPath& dst_path() const;
    inline const XIAPath& src_path() const;

protected:
    void update_hdr();

private:
    struct click_xia* _hdr;

    XIAPath _dst_path;     // copy of dstination path
    XIAPath _src_path;      // copy of source path
};

inline const struct click_xia&
XIAHeader::hdr() const
{
    return *_hdr;
}

inline struct click_xia&
XIAHeader::hdr()
{
    return *_hdr;
}

inline
XIAHeader::operator struct click_xia() const
{
    return hdr();
}

inline size_t
XIAHeader::size() const
{
    return size(_hdr->dnode + _hdr->snode);
}

inline size_t
XIAHeader::size(uint8_t dsnode)
{
    return sizeof(struct click_xia) + sizeof(struct click_xia_xid_node) * dsnode;
}

inline const XIAPath&
XIAHeader::dst_path() const
{
    return _dst_path;
}

inline const XIAPath&
XIAHeader::src_path() const
{
    return _src_path;
}

CLICK_ENDDECLS
#endif
