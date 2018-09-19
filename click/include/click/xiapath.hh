// -*- c-basic-offset: 4; related-file-name: "../../lib/xiapath.cc" -*-
#ifndef CLICK_XIAPATH_HH
#define CLICK_XIAPATH_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/vector.hh>
#include <click/xid.hh>
#include <click/dagaddr.hpp>

#define INVALID_NODE_HANDLE 2048

CLICK_DECLS
class Element;

class XIAPath { public:
    XIAPath();
    XIAPath(const XIAPath& r);
	XIAPath(const Graph& r);

    XIAPath& operator=(const XIAPath& r);

    // parse a string representation prefixed by its type (DAG or RE)
    bool parse(const String& s, const Element* context = NULL);

    // parse a DAG string representation
    bool parse_dag(const String& s, const Element* context = NULL);

    // parse a RE string representation
    bool parse_re(const String& s, const Element* context = NULL);

    // parse a node list (in the XIA header format)
    template <typename InputIterator>
    void parse_node(InputIterator node_begin, InputIterator node_end);

    template <typename InputIterator>
    void parse_node(InputIterator node_begin, size_t n);

    // unparse to a string representation prefixed by its type
    String unparse(const Element* context = NULL);

    // unparse to a DAG string representation
    String unparse_dag(const Element* context = NULL);

    // size of unparsed node list
    size_t unparse_node_size() const;

    // unparse to a node list (in the XIA header format)
    size_t unparse_node(struct click_xia_xid_node* node, size_t n) const;

    //// path access methods

    typedef size_t handle_t;

    // check if the path is valid
    bool is_valid() const;

    // get the handle of the destination node
    handle_t destination_node() const;

	// Get the intent AD as a string
	std::string intent_ad_str() const;

	// Get the intent HID as a string
	std::string intent_hid_str() const;

	// Get the intent SID as a string
	std::string intent_sid_str() const;

    // get XID of the node
    XID xid(handle_t node) const;

	// First child node for the given node
	handle_t first_hop_from_node(handle_t node) const;

	// Replace intent HID node with a new one
	bool replace_intent_hid(XID new_hid);

    //// path manipulation methods

	// append a node to the end of this DAG
	bool append_node(const XID& xid);

	// Remove intent node without consideration for type
	bool remove_intent_node();

	// Remove intent SID node from dag
	bool remove_intent_sid_node();

    // set the source node
    void set_source_node(handle_t node);

	// Find all FID type nodes
	void find_nodes_of_type(uint32_t type, Vector<XID> &v);

	// if first edge of DAG points to intent, delete edge
	// forcing us to take the fallback path
	bool flatten();

	// Compare two XIAPath objects but ignore intent AD
	int compare_except_intent_ad(XIAPath& other);

	// Check if the first hop from source node is an SID
	bool first_hop_is_sid() const;

	const Graph& get_graph() const;

	// Compare two XIAPath objects for equality
	bool operator== (XIAPath& other);

	bool operator!= (XIAPath& other);

private:

    Graph g;
};

CLICK_ENDDECLS
#endif
