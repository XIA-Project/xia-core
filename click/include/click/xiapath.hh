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

    // unparse to a RE string representation
    //String unparse_re(const Element* context = NULL);

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

	// get the handle of the first AD node in DAG towards destination
	//handle_t first_ad_node() const;

    // get XID of the node
    XID xid(handle_t node) const;

	// First child node for the given node
	handle_t first_hop_from_node(handle_t node) const;

	// Find the intent HID - last HID in path to dest node
	//handle_t find_intent_hid();

	// Find the intent SID - last SID in path to dest node
	//handle_t find_intent_sid();

	// Replace intent HID node with a new one
	bool replace_intent_hid(XID new_hid);

    // get handles of connected (next) nodes to the node
    //Vector<handle_t> next_nodes(handle_t node) const;

    //// path manipulation methods

	// append a node to the end of this DAG
	bool append_node(const XID& xid);

	/*
    // add a new node
    // returns the handle of the new node
    handle_t add_node(const XID& xid);

    // connect two nodes with an prioritized edge
    // priority of 0 is the highest
    bool add_edge(handle_t from_node, handle_t to_node, size_t priority = static_cast<size_t>(-1));
	*/

    // remove a node (will invalidate handles)
    //bool remove_node(handle_t node);

	// Remove intent node without consideration for type
	bool remove_intent_node();

	// Remove intent SID node from dag
	bool remove_intent_sid_node();

    // set the source node
    void set_source_node(handle_t node);

	/*
    // set the destination node
    void set_destination_node(handle_t node);
	*/

	// if first edge of DAG points to intent, delete edge
	// forcing us to take the fallback path
	bool flatten();

    // debug
    //void dump_state() const;

	int compare_except_intent_ad(XIAPath& other);
	// Compare two XIAPath objects but allow a named XID exception
	//int compare_with_exception(XIAPath& other, XID& my_ad, XID& their_ad);

	// Check if the first hop from source node is an SID
	bool first_hop_is_sid() const;

	const Graph& get_graph() const;
	// Compare two XIAPath objects for equality
	//int compare(XIAPath& other);

	bool operator== (XIAPath& other);

	bool operator!= (XIAPath& other);

private:

    struct Node {
        XID xid;
        Vector<handle_t> edges;
        size_t order;              // the topological order of the node in the graph
        Node() : order(static_cast<size_t>(-1)) {}
    };

    static const handle_t _npos = static_cast<handle_t>(-1);

    Vector<Node> _nodes;
    handle_t _src;
    handle_t _dst;
    Graph g;
};

CLICK_ENDDECLS
#endif
