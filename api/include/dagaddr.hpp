#pragma once

/*! \cond */
 #define PATH_SIZE 4096
 #include <unistd.h>
 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>


#include <stdint.h>	// for non-c++0x
#include <vector>
#include <string>
#include <map>

#if defined(CLICK_USERLEVEL) || defined(CLICK_TOOL)
#include <clicknet/xia.h>
#else
#include "xia.h"
#endif
/*! \endcond */

class Graph;

/*!
 * @brief Class defining XIDs that are used by the Graph class.
 */
class Node
{
public:
	static const std::size_t ID_LEN = 20;

	static const std::string XID_TYPE_UNKNOWN_STRING;
	static const std::string XID_TYPE_DUMMY_SOURCE_STRING;
	static const std::string XID_TYPE_AD_STRING;
	static const std::string XID_TYPE_HID_STRING;
	static const std::string XID_TYPE_CID_STRING;
	static const std::string XID_TYPE_NCID_STRING;
	static const std::string XID_TYPE_SID_STRING;
	static const std::string XID_TYPE_FID_STRING;
	static const std::string XID_TYPE_IP_STRING;

public:
	Node();
	Node(const Node& r);
	Node(uint32_t type, const void* id, int dummy); // NOTE: dummy is so compiler doesn't complain about ambigous constructors  TODO: fix.
	Node(int type, const std::string id_string); // NOTE: type is an int here because the consts above are ints, otherwise swig will complain again
	Node(const std::string type_string, const std::string id_string);
	Node(const std::string node_string);

	~Node();

	const uint32_t& type() const { return ptr_->type; }
	const unsigned char* id() const { return ptr_->id; }
	std::string type_string() const;
	std::string id_string() const;
	std::string to_string() const;
	bool has_valid_xid() const;

	Node& operator=(const Node& r);
	bool operator==(const Node& r) const { return ptr_ == r.ptr_; }
	bool operator!=(const Node& r) const { return ptr_ != r.ptr_; }
	Graph operator*(const Node& r) const;
	Graph operator*(const Graph& r) const;
	Graph operator+(const Node& r) const;
	Graph operator+(const Graph& r) const;

	bool equal_to(const Node& r) const;

private:
	typedef std::map<int, std::string> XidMap;
	static XidMap xids;
	static XidMap load_xids();



	void acquire() const;
	void release() const;

	struct container
	{
		uint32_t type;
		unsigned char id[ID_LEN];
		std::size_t ref_count;
	};

	mutable container* ptr_;
	static container dummy_source_;

	void construct_from_strings(const std::string type_string, const std::string id_string);
};

/*!
 * @brief Build DAGs for use as addresses in XIA
 *
 * This class provides the user a simple set of operators to build a DAG
 * for use as address within XIA.
 *
 * NOTE:
 * ----
 * Create Node(s) first and then build Graph from them
 * Node objects are compared by reference. So even if two Nodes have
 * the same XID, they are considered separate. This allows us to build
 * Graphs with the same XID appearing more than once.
 *
 */
class Graph
{
public:
	Graph();
	Graph(const Node& n);
	Graph(const Graph& r);
	Graph(std::string dag_string);
	Graph(const sockaddr_x *s);

	// Graph manipulation operations
	//
	// NOTE: Nodes are compared by reference not value
	//
	// So create Node objects first, then build Graph
	//
	Graph& operator=(const Graph& r);
	Graph& operator*=(const Graph& r);
	Graph operator*(const Graph& r) const;
	Graph& operator+=(const Graph& r);
	Graph operator+(const Graph& r) const;
	Graph operator*(const Node& r) const;
	Graph operator+(const Node& r) const;
	bool operator==(const Graph& r) const;
	void append_node_str(std::string node_str);

	static const std::size_t MAX_XIDS_IN_ALL_PATHS = 30;
	static const std::size_t INVALID_GRAPH_INDEX = 255;
	void print_graph() const;
	std::string http_url_string() const;
	std::string dag_string() const;
	bool has_intent_AD() const;
	const Node& intent_AD() const;
	const Node& intent_HID() const;
	const Node& intent_SID() const;
	const Node& intent_CID() const;
	std::string intent_AD_str() const;
	std::string intent_HID_str() const;
	std::string intent_SID_str() const;
	std::string intent_CID_str() const;
	bool is_valid() const;
	bool is_final_intent(const Node& n);
	bool is_final_intent(const std::string xid_string);
	Graph next_hop(const Node& n);
	Graph next_hop(const std::string xid_string);
	Graph first_hop();
	uint8_t num_nodes() const;
	Node get_node(int i) const;
	size_t fill_wire_buffer(node_t *buf) const;
	void fill_sockaddr(sockaddr_x *s) const;
	void from_wire_format(uint8_t num_nodes, const node_t *buf);
	void from_sockaddr(const sockaddr_x *s);
	void replace_final_intent(const Node& new_intent);
	Node get_final_intent() const;
	bool replace_intent_HID(std::string new_hid_str);
	bool replace_intent_AD(std::string new_ad_str);
	size_t unparse_node_size() const;
	bool flatten();
	bool first_hop_is_sid() const;
	bool remove_intent_sid_node();
	bool remove_intent_node();
	std::vector<const Node*> get_nodes_of_type(unsigned int type) const;

	// TODO: We need to stop exposing internal Graph indices with these
	// Need to refactor code that uses these functions
	std::string xid_str_from_index(std::size_t node) const;
	std::size_t final_intent_index() const;

	int compare_except_intent_AD(Graph other) const;
private:
	bool replace_intent_XID(uint32_t xid_type, std::string new_xid_str);
	std::size_t intent_XID_index(uint32_t xid_type) const;
	std::size_t intent_AD_index() const;
	std::size_t intent_HID_index() const;
	std::size_t intent_SID_index() const;
	std::size_t intent_CID_index() const;

	std::size_t add_node(const Node& p, bool allow_duplicate_nodes = false);
	void add_edge(std::size_t from_id, std::size_t to_id);

	void replace_node_at(int i, const Node& new_node);

	std::vector<std::size_t> get_out_edges(int i) const;

	bool is_source(std::size_t id) const;
	bool is_sink(std::size_t id) const;

	std::size_t source_index() const;

	void merge_graph(const Graph& r, std::vector<std::size_t>& node_mapping, bool allow_duplicate_nodes = false);

	std::string out_edges_for_index(std::size_t i, std::size_t source_index, std::size_t sink_index) const;
	std::size_t index_in_dag_string(std::size_t index, std::size_t source_index, std::size_t sink_index) const;
	std::size_t index_from_dag_string_index(int32_t dag_string_index, std::size_t source_index, std::size_t sink_index) const;

	void construct_from_http_url_string(std::string dag_string);
	void construct_from_dag_string(std::string dag_string);
	int check_dag_string(std::string dag_string);
	void construct_from_re_string(std::string re_string);
	int check_re_string(std::string re_string);

	bool depth_first_walk(int node, std::vector<Node> &paths) const;
	bool ordered_paths_to_sink(std::vector<Node> &paths_to_sink) const;

	void dump_stack_trace() const;
	std::vector<Node> nodes_;
	std::vector<std::vector<std::size_t> > out_edges_;
	std::vector<std::vector<std::size_t> > in_edges_;
};

/**
 * @brief Make a graph by appending a node
 *
 * Make a graph by appending a node to this node. The resulting graph will have
 * one edge from this node to the supplied node.
 *
 * @param r The node to append
 *
 * @return The resulting graph
 */
inline Graph Node::operator*(const Node& r) const
{
	return Graph(*this) * Graph(r);
}

/**
* @brief Append a node
*
* Append a node to the end of this graph and return the result.
*
* @param r The node to append
*
* @return The resulting graph
*/
inline Graph Graph::operator*(const Node& r) const
{
	return Graph(*this) * Graph(r);
}

/**
* @brief Make a graph by appending a graph
*
* Make a graph by appending an existing graph to this node. The resulting
* graph will have an edge from this node to the source node of the supplied
* one. The edges of the supplied graph will not be affected.
*
* @param r The graph to append
*
* @return The resulting graph
*/
inline Graph Node::operator*(const Graph& r) const
{
	return Graph(*this) * r;
}

/**
* @brief Make a graph by merging with a node
*
* Make a graph by merging this node with another node. If the nodes are equal,
* the resulting graph will contain one node and no edges. Otherwise, the
* resulting graph will contain two nodes and no edges. Each node will be both
* a source and a sink.
*
* @param r The node with which to merge
*
* @return The resulting graph
*/
inline Graph Node::operator+(const Node& r) const
{
	return Graph(*this) + Graph(r);
}

/**
* @brief Merge with a node
*
* Merge the supplied node with this graph. If the node is already present in
* this graph, the resulting graph will be the same. If the node is different,
* the new node will be added to the graph but will not be connected by any
* edges.
*
* @param r The node with which to merge
*
* @return The resulting graph
*/
inline Graph Graph::operator+(const Node& r) const
{
	return Graph(*this) + Graph(r);
}

/**
* @brief Make a graph by merging with a graph
*
* Make a graph by merging this node with the supplied graph. If the node is
* already present in this graph, the resulting graph will be the same as the
* supplied graph. If the node is different, this node will be added to the
* graph but will not be connected by any edges.
*
* @param r The graph with which to merge
*
* @return The resulting graph
*/
inline Graph Node::operator+(const Graph& r) const
{
	return Graph(*this) + r;
}
