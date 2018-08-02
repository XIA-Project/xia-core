// -*- related-file-name: "../include/click/xiapath.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiapath.hh>
#include <click/confparse.hh>
#include <click/straccum.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
CLICK_DECLS

#define NDEBUG_XIA

XIAPath::XIAPath()
{
}

XIAPath::XIAPath(const XIAPath& r)
{
    g = r.g;
}

XIAPath&
XIAPath::operator=(const XIAPath& r)
{
    g = r.g;
    return *this;
}

bool
XIAPath::parse(const String& s, const Element* context)
{
    (void) context;
    g = Graph(s.c_str());
    if(g.num_nodes() <= 1) {
        return false;
    }
    return true;
}

bool
XIAPath::parse_dag(const String& s, const Element* context)
{
    (void) context;
    return parse(s, context);
}

bool
XIAPath::parse_re(const String& s, const Element* context)
{
    (void) context;
    return parse(s, context);
}

template <typename InputIterator>
void
XIAPath::parse_node(InputIterator node_begin, InputIterator node_end)
{
    if (node_begin == node_end)
        return;

    int count = 0;
    InputIterator current_node = node_begin;

    while (current_node != node_end) {
        ++count;
        ++current_node;
    }
    g.from_wire_format(count, node_begin);
}

template void XIAPath::parse_node(const struct click_xia_xid_node*, const struct click_xia_xid_node*);
template void XIAPath::parse_node(struct click_xia_xid_node*, struct click_xia_xid_node*);

template <typename InputIterator>
void
XIAPath::parse_node(InputIterator node_begin, size_t n)
{
    g.from_wire_format(n, node_begin);
}

template void XIAPath::parse_node(const struct click_xia_xid_node*, size_t);
template void XIAPath::parse_node(struct click_xia_xid_node*, size_t);

String
XIAPath::unparse(const Element* context)
{
    (void) context;
    return g.dag_string().c_str();
}

String
XIAPath::unparse_dag(const Element* context)
{
    (void) context;
    return g.dag_string().c_str();
}

size_t
XIAPath::unparse_node_size() const
{
    return g.unparse_node_size();
}

size_t
XIAPath::unparse_node(struct click_xia_xid_node* node, size_t n) const
{
    size_t total_n = unparse_node_size();
    if (n > total_n)
        n = total_n;

    if (total_n == 0)
        return 0;

    return g.fill_wire_buffer(node);
}


bool
XIAPath::is_valid() const
{
    if (g.num_nodes() >= 1) {
        return true;
    }
    return false;
}

void
XIAPath::find_nodes_of_type(uint32_t type, Vector<XID> &v)
{
    std::vector<const Node *>fid_nodes = g.get_nodes_of_type(type);
    for (size_t i=0; i < fid_nodes.size(); i++) {
        v.push_back(XID(fid_nodes[i]->to_string().c_str()));
    }
}

XIAPath::handle_t
XIAPath::destination_node() const
{
    return g.final_intent_index();
}

std::string
XIAPath::intent_ad_str() const
{
    return g.intent_AD_str();
}

std::string
XIAPath::intent_hid_str() const
{
    return g.intent_HID_str();
}

/**
  * @brief return XID corresponding to handle 'node'
  *
  * 'node' is an index into the internal Graph.
  * 
  * @return XID corresponding to the given handle
  *
  */
XID
XIAPath::xid(handle_t node) const
{
    return XID(String(g.xid_str_from_index(node).c_str()));
}

bool
XIAPath::replace_intent_hid(XID new_hid)
{
    return g.replace_intent_HID(new_hid.unparse().c_str());
}

std::string
XIAPath::intent_sid_str() const
{
    return g.intent_SID_str();
}

bool
XIAPath::append_node(const XID& xid)
{
    g.append_node_str(xid.unparse().c_str());
    return true;
}

bool
XIAPath::remove_intent_sid_node()
{
    return g.remove_intent_sid_node();
}

bool
XIAPath::remove_intent_node()
{
    return g.remove_intent_node();
}


bool
XIAPath::flatten()
{
    return g.flatten();
}

int
XIAPath::compare_except_intent_ad(XIAPath& other)
{
    return g.compare_except_intent_AD(other.get_graph());
}

const Graph&
XIAPath::get_graph() const
{
    return g;
}

bool
XIAPath::first_hop_is_sid() const
{
    return g.first_hop_is_sid();
}

bool XIAPath::operator== (XIAPath& other) {
    return (g == other.g);
}

bool XIAPath::operator!= (XIAPath& other) {
    return !(g == other.g);
}

#ifndef NDEBUG_XIA
struct XIASelfTest {
    XIASelfTest()
    {
        // alignment/packing test
        {
            assert(sizeof(struct click_xia) == 8);
            assert(sizeof(struct click_xia_xid) == 24);
            assert(sizeof(struct click_xia_xid_edge) == 1);
            assert(sizeof(struct click_xia_xid_node) == 28);
            struct click_xia hdr;
            assert(reinterpret_cast<unsigned long>(&(hdr.node[0])) - reinterpret_cast<unsigned long>(&hdr) == 8);
            assert(reinterpret_cast<unsigned long>(&(hdr.node[1])) - reinterpret_cast<unsigned long>(&hdr) == 8 + 28);
            assert(reinterpret_cast<unsigned long>(&(hdr.node[2])) - reinterpret_cast<unsigned long>(&hdr) == 8 + 56);

            assert(sizeof(struct click_xia_ext) == 2);
            struct click_xia_ext ext_hdr;
            assert(reinterpret_cast<unsigned long>(&(ext_hdr.data[0])) - reinterpret_cast<unsigned long>(&ext_hdr) == 2);
            assert(reinterpret_cast<unsigned long>(&(ext_hdr.data[1])) - reinterpret_cast<unsigned long>(&ext_hdr) == 3);
            assert(reinterpret_cast<unsigned long>(&(ext_hdr.data[2])) - reinterpret_cast<unsigned long>(&ext_hdr) == 4);
        }

        // RE test
        /*
        {
            const char* s[] = {
                "AD:0000000000000000000000000000000000000000",
                "AD:0000000000000000000000000000000000000000 AD:0000000000000000000000000000000000000001",
                "AD:0000000000000000000000000000000000000000 ( AD:0000000000000000000000000000000000000002 ) AD:0000000000000000000000000000000000000001",
                "AD:0000000000000000000000000000000000000000 ( AD:0000000000000000000000000000000000000002 AD:0000000000000000000000000000000000000003 ) AD:0000000000000000000000000000000000000001",
            };
            for (size_t i = 0; i < sizeof(s) / sizeof(s[0]); i++)
            {
                XIAPath p;
                assert(p.parse_re(s[i]));
                //click_chatter("%s\n", p.unparse_re().c_str());
                assert(p.unparse_re() == s[i]);
            }
        }
        */

        // DAG test
        {
            const char* s[] = {
                "0 - AD:0000000000000000000000000000000000000000",
                "0 - AD:0000000000000000000000000000000000000000 1 - AD:0000000000000000000000000000000000000001",
                "0 - AD:0000000000000000000000000000000000000000 1 - AD:0000000000000000000000000000000000000001 2 - AD:0000000000000000000000000000000000000002",
            };
            for (size_t i = 0; i < sizeof(s) / sizeof(s[0]); i++)
            {
                XIAPath p;
                assert(p.parse_dag(s[i]));
                //click_chatter("%s\n", p.unparse_dag().c_str());
                assert(p.unparse_dag() == s[i]);
            }
        }

        // path manipulation test
        /*
        {
            XIAPath p;
            XIAPath::handle_t src_node = p.add_node(XID());
            p.set_source_node(src_node);
            XIAPath::handle_t dst_node = p.add_node(XID("AD:0000000000000000000000000000000000000000"));
            p.set_destination_node(dst_node);
            p.add_edge(src_node, dst_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            XIAPath::handle_t prev_dst_node = dst_node;
            dst_node = p.add_node(XID("AD:0000000000000000000000000000000000000001"));
            p.set_destination_node(dst_node);
            p.add_edge(prev_dst_node, dst_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            XIAPath::handle_t fallback_node = p.add_node(XID("AD:0000000000000000000000000000000000000002"));
            p.add_edge(prev_dst_node, fallback_node);
            p.add_edge(fallback_node, dst_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            XIAPath::handle_t fallback_node2 = p.add_node(XID("AD:0000000000000000000000000000000000000003"));
            p.add_edge(fallback_node, fallback_node2);
            p.add_edge(fallback_node2, dst_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            p.remove_node(fallback_node2);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            p.remove_node(fallback_node);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());

            p.add_edge(src_node, dst_node, 0);
            assert(p.is_valid());
            click_chatter("%s\n", p.unparse_re().c_str());
        }
        */

        click_chatter("test passed.\n");
    }
};

static XIASelfTest _self_test;
#endif

CLICK_ENDDECLS
