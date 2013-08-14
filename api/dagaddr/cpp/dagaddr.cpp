/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
/*!
 @file dagaddr.cpp
 @brief Implements dagaddr library
*/
#include "dagaddr.hpp"
#include "utils.hpp"
#include <cstring>
#include <cstdio>
#include <map>
#include <algorithm>


static const std::size_t vector_find_npos = std::size_t(-1);

template <typename T>
static std::size_t
vector_find(std::vector<T>& v, const T& e)
{
	for (std::size_t i = 0; i < v.size(); i++)
		if (v[i] == e)
			return i;
	return vector_find_npos;
}

template <typename T>
static std::size_t
vector_push_back_unique(std::vector<T>& v, const T& e)
{
	std::size_t idx = vector_find(v, e);
	if (idx == vector_find_npos)
	{
		idx = v.size();
		v.push_back(e);
	}
	return idx;
}




Node::container Node::dummy_source_ = {0xff, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 0};

const std::string Node::XID_TYPE_UNKNOWN_STRING = "UNKNOWN";
const std::string Node::XID_TYPE_DUMMY_SOURCE_STRING = "SOURCE";
const std::string Node::XID_TYPE_AD_STRING = "AD";
const std::string Node::XID_TYPE_HID_STRING = "HID";
const std::string Node::XID_TYPE_CID_STRING = "CID";
const std::string Node::XID_TYPE_SID_STRING = "SID";
const std::string Node::XID_TYPE_IP_STRING = "IP";


/**
* @brief Create a new empty node.
*
* Create a new empty node with type XID_TYPE_DUMMY_SOURCE and id 0. This is
* 			commonly used to create the "dummy" source node.
*/
Node::Node()
	: ptr_(&dummy_source_)
{
}

/**
* @brief Create a copy of a node
*
* Create a new node that is a copy of the supplied node.
*
* @param r The node to be copied.
*/
Node::Node(const Node& r)
	: ptr_(r.ptr_)
{
	acquire();
}

Node::Node(uint32_t type, const void* id, int dummy)
{
	ptr_ = new container;
	ptr_->type = type;
	memcpy(ptr_->id, id, ID_LEN);
	ptr_->ref_count = 1;
}

/**
* @brief Create a new node from a type and an XID string
*
* Create a new DAG node from a principal type and an XID (as a string).
*
* @param type The node's principal type. Must be one of:
* 			\n Node::XID_TYPE_AD (Administrative Domain)
*			\n Node::XID_TYPE_HID (Host)
*			\n Node::XID_TYPE_CID (Content)
*			\n Node::XID_TYPE_SID (Service)
*			\n Node::XID_TYPE_IP (IPv4 / 4ID)
*
* @param id_str The node's XID as a string. This should be 20 pairs of two
*			characters; each pair of characters is the ASCII representation
*			of one byte of the XID in hex.
*/
Node::Node(int type, const std::string id_str)
{
	ptr_ = new container;
	ptr_->ref_count = 1;
	ptr_->type = type;

	for (std::size_t i = 0; i < ID_LEN; i++)
	{
		int num = stoi(id_str.substr(2*i, 2), 0, 16);
		memcpy(&(ptr_->id[i]), &num, 1);
	}
}

/**
* @brief Create a new node from a type string and an XID string
*
* Create a new DAG node from a principal type (as a string)
* and an XID (as a string).
*
* @param type_str The node's prinicipal type (as a string). Must be one of:
*			\n Node::XID_TYPE_AD_STRING
*			\n Node::XID_TYPE_HID_STRING
*			\n Node::XID_TYPE_CID_STRING
*			\n Node::XID_TYPE_SID_STRING
*			\n Node::XID_TYPE_IP_STRING
*
* @param id_str The node's XID as a string. This should be 20 pairs of two
*			characters; each pair of characters is the ASCII representation
*			of one byte of the XID in hex.
*/
Node::Node(const std::string type_str, const std::string id_str)
{
	construct_from_strings(type_str, id_str);
}


/**
* @brief Create a new node from a single string.
*
* Create a new node from a single string with the format <type>:<id>.
* Examples:
*	\n AD:1234567890123456789012345678901234567890
*	\n CID:0000011111222223333344444555556666677777
*
* @param node_string
*/
Node::Node(const std::string node_string)
{
	// TODO: check string is valid format
	std::vector<std::string> xid_elems = split(node_string, ':');
	construct_from_strings(xid_elems[0], xid_elems[1]);
}

Node::~Node()
{
	release();
}

Node&
Node::operator=(const Node& r)
{
	release();
	ptr_ = r.ptr_;
	acquire();
	return *this;
}

bool
Node::equal_to(const Node& r) const
{
	return ptr_->type == r.ptr_->type && memcmp(ptr_->id, r.ptr_->id, ID_LEN) == 0;
}

void
Node::acquire() const
{
	if (ptr_ == &dummy_source_)
		return;
	++ptr_->ref_count;
}

void
Node::release() const
{
	if (ptr_ == &dummy_source_)
		return;
	if (--ptr_->ref_count == 0)
	{
		delete ptr_;
		ptr_ = &dummy_source_;
	}
}


void
Node::construct_from_strings(const std::string type_str, const std::string id_str)
{
	ptr_ = new container;
	ptr_->ref_count = 1;

	if (type_str == XID_TYPE_AD_STRING)
		ptr_->type = XID_TYPE_AD;
	else if (type_str == XID_TYPE_HID_STRING)
		ptr_->type = XID_TYPE_HID;
	else if (type_str == XID_TYPE_CID_STRING)
		ptr_->type = XID_TYPE_CID;
	else if (type_str == XID_TYPE_SID_STRING)
		ptr_->type = XID_TYPE_SID;
	else if (type_str == XID_TYPE_IP_STRING)
		ptr_->type = XID_TYPE_IP;
	else if (type_str == XID_TYPE_DUMMY_SOURCE_STRING)
		ptr_->type = XID_TYPE_DUMMY_SOURCE;
	else
		ptr_->type = 0;

	for (std::size_t i = 0; i < ID_LEN; i++)
	{
		int num = stoi(id_str.substr(2*i, 2), 0, 16);
		memcpy(&(ptr_->id[i]), &num, 1);
	}
}

/**
* @brief Return the node's type as a string
*
* @return The node's type as a string. Will be one of:
*			\n Node::XID_TYPE_AD_STRING
*			\n Node::XID_TYPE_HID_STRING
*			\n Node::XID_TYPE_CID_STRING
*			\n Node::XID_TYPE_SID_STRING
*			\n Node::XID_TYPE_IP_STRING
*			\n Node::XID_TYPE_DUMMY_SOURCE
*			\n Node:: ID_TYPE_UNKNOWN_STRING
*/
std::string
Node::type_string() const
{
	switch(this->type())
	{
		case XID_TYPE_AD:
			return XID_TYPE_AD_STRING;
		case XID_TYPE_HID:
			return XID_TYPE_HID_STRING;
		case XID_TYPE_CID:
			return XID_TYPE_CID_STRING;
		case XID_TYPE_SID:
			return XID_TYPE_SID_STRING;
		case XID_TYPE_IP:
			return XID_TYPE_IP_STRING;
		case XID_TYPE_DUMMY_SOURCE:
			return XID_TYPE_DUMMY_SOURCE_STRING;
		default:
			return XID_TYPE_UNKNOWN_STRING;
	}
}

/**
* @brief Get the node's XID as a string
*
* Get the node's XID as a std::string. This string will be 40 characters long
* (representing 20 bytes in hex). The string does not include the principal 
* type prefix (e.g., "HID:").
*
* @return The node's XID as a string
*/
std::string
Node::id_string() const
{
	std::string xid_string;
	char *xid = (char*)malloc(sizeof(char) * 2 * Node::ID_LEN + 1);
	for (std::size_t j = 0; j < Node::ID_LEN; j++)
		snprintf(&xid[2*j], 3, "%02x", id()[j]);
	xid_string += xid;
	free(xid);
	return xid_string;
}

/**
* @brief Create an empty graph
*
* Create an empty graph.
*/
Graph::Graph()
{
}

/**
* @brief Create a graph from a node
*
* Create a graph containing a single node.
*
* @param n The node the new graph should contain.
*/
Graph::Graph(const Node& n)
{
	add_node(n);
}

/**
* @brief Create a copy of a graph
*
* Create a new graph that is a copy of the supplied graph.
*
* @param r The graph to be copied.
*/
Graph::Graph(const Graph& r)
{
	nodes_ = r.nodes_;
	out_edges_ = r.out_edges_;
	in_edges_ = r.in_edges_;
}

/**
* @brief Create a graph from a DAG string
*
* Create a new graph from a DAG string. It is generally recommended not to write
* DAG strings from scratch, but to obtain them by creating Graphs with other
* constructors and calling Graph::dag_string(). This constructor is also useful
* for creating a DAG from a DAG string obtained from the XSocket API (from the
* XrecvFrom() call, for example).
*
* @param dag_string The DAG in the string format used by the XSocket API.
*			Example 1:
*<div><pre>DAG 2 0 - 
*AD:4349445f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f 2 1 - 
*HID:4849445f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f 2 - 
*SID:534944305f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f</pre></div>
*			Example 2:
*<div><pre>RE ( AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000 ) SID:1110000000000000000000000000000000001113</pre></div>
*
*/
Graph::Graph(std::string dag_string)
{
	if (dag_string.find("DAG") != std::string::npos)
	{
		construct_from_dag_string(dag_string);
	}
	else if (dag_string.find("RE") != std::string::npos)
	{
		construct_from_re_string(dag_string);
	}
	else
	{
		printf("WARNING: dag_string must be in either DAG or RE format. Returning empty Graph.\n");
		add_node(Node());
	}
}
	
/**
* @brief Create a graph from a sockaddr_x
*
* Create a new graph from a sockaddr_x. This constructor is useful for creating
* a DAG from from a sockaddr_x obtained from the XSocket API (from the
* XrecvFrom() call, for example).
*
* @param s The sockaddr_x
*/
Graph::Graph(const sockaddr_x *s)
{
	from_sockaddr(s);
}

Graph&
Graph::operator=(const Graph& r)
{
	nodes_ = r.nodes_;
	out_edges_ = r.out_edges_;
	in_edges_ = r.in_edges_;
	return *this;
}

Graph&
Graph::operator*=(const Graph& r)
{
	std::vector<std::size_t> sinks;
	std::vector<std::size_t> sources;

	for (std::size_t i = 0; i < nodes_.size(); i++)
		if (is_sink(i))
			sinks.push_back(i);

	for (std::size_t i = 0; i < r.nodes_.size(); i++)
		if (r.is_source(i))
		{
			if (r.nodes_[i].type() == Node::XID_TYPE_DUMMY_SOURCE)
			{
				for (std::vector<std::size_t>::const_iterator it = r.out_edges_[i].begin(); it != r.out_edges_[i].end(); ++it)
					vector_push_back_unique(sources, *it);
			}
			else
			{
				sources.push_back(i);
			}
		}

	std::vector<std::size_t> node_mapping_r;
	merge_graph(r, node_mapping_r, true);

	for (std::vector<std::size_t>::const_iterator it_sink = sinks.begin(); it_sink != sinks.end(); ++it_sink)
		for (std::vector<std::size_t>::const_iterator it_source = sources.begin(); it_source != sources.end(); ++it_source)
			add_edge(*it_sink, node_mapping_r[*it_source]);

	return *this;
}

/**
* @brief Append a graph to this one
*
* Append the supplied graph to the end of this one and return the resulting graph.
*
* @param r The graph to be appended
*
* @return The resulting graph
*/
Graph
Graph::operator*(const Graph& r) const
{
	return Graph(*this) *= r;
}

Graph&
Graph::operator+=(const Graph& r)
{
	std::vector<std::size_t> node_mapping_r;
	merge_graph(r, node_mapping_r);
	return *this;
}

/**
* @brief Merge a graph with this one
*
* Merge the supplied graph with this one and return the resulting graph. If
* both graphs share the same source and sink, one graph will become a fallback path.
*
* @param r The graph to be merged with this one
*
* @return The resulting graph
*/
Graph
Graph::operator+(const Graph& r) const
{
	return Graph(*this) += r;
}

/**
* @brief Print the graph
*
* Print the graph. Useful for debugging.
*
* @warning The format in which this function prints a graph is not the same
* 			as the DAG string format used by the XSocket API. To obtain a 
*			string version of the DAG for use with the XSocket API, use
*			Graph::dag_string() instead.
*/
void
Graph::print_graph() const
{
	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (is_source(i))
			printf("[SRC] ");
		else
			printf("      ");

		printf("Node %zu: [%s] ", i, nodes_[i].type_string().c_str());
		//printf("%20s", nodes_[i].id());
		for (std::size_t j = 0; j < Node::ID_LEN; j++)
			printf("%02x", nodes_[i].id()[j]);
		bool first = true;
		for (std::size_t j = 0; j < out_edges_[i].size(); j++)
		{
			if (first)
			{
				first = false;
				printf(" ->");
			}
			printf(" Node %zu", out_edges_[i][j]);
		}
		if (is_sink(i))
			printf(" [SNK]");
		printf("\n");
	}
}

std::size_t
Graph::add_node(const Node& p, bool allow_duplicate_nodes)
{
	std::size_t idx;
	if (allow_duplicate_nodes) {
		nodes_.push_back(p);
		idx = nodes_.size()-1;
	} else {
		idx = vector_push_back_unique(nodes_, p);
	}

	if (idx >= out_edges_.size())
	{
		out_edges_.push_back(std::vector<std::size_t>());
		in_edges_.push_back(std::vector<std::size_t>());
	}
	return idx;
}

void
Graph::add_edge(std::size_t from_id, std::size_t to_id)
{
	if (from_id == to_id)
		return;
	vector_push_back_unique(out_edges_[from_id], to_id);
	vector_push_back_unique(in_edges_[to_id], from_id);
}

bool
Graph::is_source(std::size_t id) const
{
	return in_edges_[id].size() == 0;
}

bool
Graph::is_sink(std::size_t id) const
{
	return out_edges_[id].size() == 0;
}

/**
* @brief Merge the supplied graph into this one.
*
* Merge the supplied graph into this one. If allow_duplicate_nodes is false,
* any XID appearing in both r and this graph will appear only once in the
* merged graph; any "extra" edges it has in r will be merged into this one.
* If allow_duplicate_nodes is true, then r is merely appended to the end of
* this graph; an XID already in this graph that appears again in r is treated
* as a "new instance" and given its own node.
*
* @param r  The graph to merge into this one.
* @param node_mapping  Initially empty; node_mapping[i] will contain node i's (in r)
*  index in the newly merged graph.
* @param allow_duplicate_nodes A bool indicating whether or not the same XID
*	appearing in both graphs should be treated as two separate nodes or merged
*	into one.
*/
void
Graph::merge_graph(const Graph& r, std::vector<std::size_t>& node_mapping, bool allow_duplicate_nodes)
{
	node_mapping.clear();

	for (std::vector<Node>::const_iterator it = r.nodes_.begin(); it != r.nodes_.end(); ++it)
		if (allow_duplicate_nodes && (*it).type() == Node::XID_TYPE_DUMMY_SOURCE) // don't add r's source node to the middle of this graph
			node_mapping.push_back(-1);
		else
		{
			node_mapping.push_back(add_node(*it, allow_duplicate_nodes));
		}

	for (std::size_t from_id = 0; from_id < r.out_edges_.size(); from_id++)
	{
		if (allow_duplicate_nodes && r.nodes_[from_id].type() == Node::XID_TYPE_DUMMY_SOURCE) continue;
		for (std::vector<std::size_t>::const_iterator it = r.out_edges_[from_id].begin(); it != r.out_edges_[from_id].end(); ++it)
			add_edge(node_mapping[from_id], node_mapping[*it]);
	}
}

std::string
Graph::out_edges_for_index(std::size_t i, std::size_t source_index, std::size_t sink_index) const
{
	std::string out_edge_string;
	for (std::size_t j = 0; j < out_edges_[i].size(); j++)
	{
		size_t idx = index_in_dag_string(out_edges_[i][j], source_index, sink_index);
		char *idx_str;
		int size = snprintf(NULL, 0, " %zu", idx);
		idx_str = (char*)malloc(sizeof(char) * size);
		sprintf(idx_str, " %zu", idx);
		out_edge_string += idx_str; 
		free(idx_str);
	}

	return out_edge_string;
}

std::size_t 
Graph::index_in_dag_string(std::size_t index, std::size_t source_index, std::size_t sink_index) const
{
	if (index == sink_index)
		return nodes_.size() - 2; // -1 because start is 0 and -1 because source is -1

	std::size_t new_index = index;
	// subtract 1 if after source index (source moves to -1)
	if (index > source_index)
		new_index--;

	// subtract 1 if after sink index (sink moves to the end)
	if (index > sink_index)
		new_index--;

	return new_index;
}
	
std::size_t 
Graph::index_from_dag_string_index(std::size_t dag_string_index, std::size_t source_index, std::size_t sink_index) const
{
	if (dag_string_index == -1) return source_index;
	if (dag_string_index == num_nodes()-1) return sink_index;

	std::size_t real_index = dag_string_index;
	if (source_index < sink_index)
	{
		// add 1 if after real sink index (sink moves back from -1)
		if (real_index >= source_index)
			real_index++;

		// add 1 if after sink index (sink moves back from end)
		if (real_index >= sink_index)
			real_index++;
	}
	else
	{
		// add 1 if after sink index (sink moves back from end)
		if (real_index >= sink_index)
			real_index++;

		// add 1 if after real sink index (sink moves back from -1)
		if (real_index >= source_index)
			real_index++;
	}

	return real_index;
}

/**
* @brief Return the graph in string form
*
* Get the DAG string representation of the graph. This string is suitable for
* use with the XSocket API.
*
* @return The graph in DAG string form.
*/
std::string
Graph::dag_string() const
{
	// TODO: check DAG first (one source, one sink, actually a DAG)
	std::string dag_string;
	int sink_index = -1, source_index = -1;

	// Find source and sink
	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (is_source(i)) 
			source_index = i;

		if (is_sink(i))
			sink_index = i;
	}

	if (sink_index >= 0 && source_index >= 0)
	{
		// Add source first
		dag_string += "DAG";
		dag_string += out_edges_for_index(source_index, source_index, sink_index);
		dag_string += " - \n";

		// Add intermediate nodes
		for (std::size_t i = 0; i < nodes_.size(); i++)
		{
			if (i == source_index || i == sink_index)
				continue;

			// add XID type
			dag_string += nodes_[i].type_string() + ":";

			// add XID
			dag_string += nodes_[i].id_string();

			// add out edges
			dag_string += out_edges_for_index(i, source_index, sink_index);
			dag_string += " - \n";
		}

		// Add sink last
		dag_string += nodes_[sink_index].type_string() + ":";
		dag_string += nodes_[sink_index].id_string();
	}
	else
	{
		printf("WARNING: dag_string(): could not find source and/or sink. Returning empty string.\n");
	}


	return dag_string;
}
	

/**
* @brief Get the index of the souce node
*
* Get the index of the source node. This method returns the index of the first
* source node it finds.
*
* @return The index of the DAG's source node
*/
std::size_t 
Graph::source_index() const
{
	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (is_source(i)) return i;
	}

	printf("Warning: source_index: no source node found\n");
	return -1;
}


/**
* @brief Get the index of the final intent node
*
* Get the index of the final intent node. This method returns the index of the
* first sink node it finds.
*
* @return The index of the DAG's final intent node.
*/
std::size_t
Graph::final_intent_index() const
{
	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (is_sink(i)) return i;
	}

	printf("Warning: source_index: no sink node found\n");
	return -1;
}

/**
* @brief Test if a node is the final intent
*
* Check whether or not the supplied Node is the final intent of the DAG
*
* @param n The node to check
*
* @return true if n is the final intent, false otherwise
*/
bool
Graph::is_final_intent(const Node& n)
{
	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (nodes_[i] == n) return is_sink(i);
	}
	
	printf("Warning: is_final_intent: supplied node not found in DAG: %s\n", n.id_string().c_str());
	return false;
}

/**
* @brief Test if a node is the final intent
*
* Check whether or not the supplied XID is the final intent of the DAG
*
* @param xid_string The XID to check
*
* @return true if the XID is the final intent, false otherwise
*/
bool
Graph::is_final_intent(const std::string xid_string)
{
	std::string xid_str = xid_string;
	// if the string includes the "<type>:", cut it off
	if (xid_string.find(":") != std::string::npos)
	{
	    xid_str = split(xid_string, ':')[1];
	}

	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (nodes_[i].id_string() == xid_str) return is_final_intent(nodes_[i]);
	}
	
	printf("Warning: is_final_intent: supplied node not found in DAG: %s\n", xid_str.c_str());
	return false;
}
	
/**
* @brief Get the next "intent unit" in the DAG starting at the supplied node
*
* Get the next "intent unit" in the DAG starting at the supplied node. An 
* intent node is a node that can be reached by following the highest priority
* out edge of each node beginning with the source. An intent unit is an intent
* node along with its fallbacks. In the new DAG, the previous intent unit
* becomes the source node.
*
* @param n The node from which to find the next intent unit. n must be an
* 		   intent node.
*
* @return The next intent unit (as a Graph)
*/
Graph
Graph::next_hop(const Node& n)
{
	// first find n and make sure it's an intent unit
	std::size_t nIndex = -1;
	std::size_t curIndex = source_index();
	while (nIndex == -1) {
		if (nodes_[curIndex] == n) {
			nIndex = curIndex;
		} else {
			if (is_sink(curIndex)) {
				printf("Warning: next_hop: n not found or is not an intent node\n");
				return Graph();
			} else {
				curIndex = out_edges_[curIndex][0];
			}
		}
	}
	
	// if n is the DAG's sink, there is no next hop
	if (is_sink(nIndex))
	{
		printf("Warning: next_hop: n is the final intent; no next hop\n");
		return Graph();
	}


	// find the next intent node (the final intent of the new DAG)
	// for now, we we'll only consider CIDs and SIDs to be intent nodes
	std::size_t intentIndex = nIndex;
	while (intentIndex == nIndex || (nodes_[intentIndex].type() != Node::XID_TYPE_CID && nodes_[intentIndex].type() != Node::XID_TYPE_SID))
	{
		if (is_sink(intentIndex)) {
			break;
		}
		intentIndex = out_edges_[intentIndex][0];

	}

	Graph g = Graph();

	// make copies of all the nodes we need in the new Graph with BFS
	std::vector<std::size_t> to_process;
	std::map<std::size_t, std::size_t> old_to_new_map;

	vector_push_back_unique(to_process, nIndex);
	while (to_process.size() > 0)
	{
		curIndex = to_process.back();
		to_process.pop_back();

		// copy this node to the new graph and the mapping
		Node new_node;
		if (curIndex != nIndex) // the start node, n, should be replaced with a dummy source
		{
			new_node = Node(nodes_[curIndex]);
		}
		std::size_t newIndex = g.add_node(new_node);
		old_to_new_map[curIndex] = newIndex;


		// prepare to process its children
		if (curIndex == intentIndex) continue; // but if we've gotten to the new intent node, stop
		for (int i = 0; i < out_edges_[curIndex].size(); i++)
		{
			vector_push_back_unique(to_process, out_edges_[curIndex][i]);
		}
	}
	
	// add edges to the new graph
	for (std::map<std::size_t, std::size_t>::const_iterator it = old_to_new_map.begin(); it != old_to_new_map.end(); ++it)
	{
		std::size_t old_index = (*it).first;
		std::size_t new_index = (*it).second;

		if (old_index == intentIndex) continue; // the new final intent has no out edges

		for (int i = 0; i < out_edges_[old_index].size(); i++)
		{
			g.add_edge(new_index, old_to_new_map[out_edges_[old_index][i]]);
		}
	}

	return g;
}

/**
* @brief Get the next "intent unit" in the DAG starting at the supplied XID
*
* This method is the same as Graph::next_hop(const Node& n) except it takes an
* XID string instead of a Node object.
*
* @param xid_string The XID of the intent node from which to find the next
*				    intent unit.
*
* @return The next intent unit (as a Graph)
*/
Graph 
Graph::next_hop(const std::string xid_string)
{
	std::string xid_str = xid_string;
	// if the string includes the "<type>:", cut it off
	if (xid_string.find(":") != std::string::npos)
	{
	    xid_str = split(xid_string, ':')[1];
	}

	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (nodes_[i].id_string() == xid_str) return next_hop(nodes_[i]);
	}
	
	printf("Warning: next_hop: supplied node not found in DAG: %s\n", xid_str.c_str());
	return Graph();

}

/**
* @brief Get the first "intent unit" in the DAG
*
* Get the first "intent unit" in the DAG. Calling this method is equivalent to
* calling g.next_hope(Node()).
*
* @return The DAG's first intent unit (as a Graph)
*/
Graph
Graph::first_hop()
{
	return next_hop(Node());
}
	
/**
* @brief Get the number of nodes in the DAG
*
* Get the number of nodes in the DAG.
*
* @note This does not include the starting node
*
* @return The number of nodes in the DAG
*/
uint8_t
Graph::num_nodes() const
{
	return nodes_.size()-1;
}


/**
* @brief Get a node from the DAG
*
* Get a Node from the graph at the specified index. The sink node will always
* be returned last (that is, it has index num_nodes()-1).
*
* @note This function skips the starting node
*
* @param i The index of the node to return
*
* @return The node at index i
*/
Node 
Graph::get_node(int i) const
{
	std::size_t src_index, sink_index;
	for (std::size_t j = 0; j < nodes_.size(); j++)
	{
		if (is_source(j)) src_index = j;
		if (is_sink(j)) sink_index = j;
	}

	return nodes_[index_from_dag_string_index(i, src_index, sink_index)];
}


/**
* @brief Get the out edges for a node
*
* Get the out edges for a node at index i
*
* @note This function skips the starting node. Use index -1 to get the
* starting node's outgoing edges.
*
* @param i The index of the node
*
* @return The out edges of node i
*/
std::vector<std::size_t>
Graph::get_out_edges(int i) const
{
	std::size_t src_index, sink_index;
	for (std::size_t j = 0; j < nodes_.size(); j++)
	{
		if (is_source(j)) src_index = j;
		if (is_sink(j)) sink_index = j;
	}

	std::size_t real_index;
	if (i == -1) 
		real_index = src_index;
	else
		real_index = index_from_dag_string_index(i, src_index, sink_index);

	std::vector<std::size_t> out_edges;
	for (std::size_t j = 0; j < out_edges_[real_index].size(); j++)
	{
		out_edges.push_back(index_in_dag_string(out_edges_[real_index][j], src_index, sink_index));
	}

	return out_edges;
}


void 
Graph::construct_from_dag_string(std::string dag_string)
{
	// remove newline chars
	dag_string.erase(std::remove(dag_string.begin(), dag_string.end(), '\n'), dag_string.end());

	// split on '-'
	std::vector<std::string> lines = split(dag_string, '-');


	// Maps indices in the graph to indices in the DAG string
	// IMPORTANT: DAG string indices are off by one since the first
	// line in a DAG string is index -1. So, when indexing into this
	// vector, ALWAYS ADD 1
	std::vector<std::size_t> dag_idx_to_graph_idx;

	// Pass 1: add nodes
	for (int i = 0; i < lines.size(); i++)
	{
		std::vector<std::string> elems = split(trim(lines[i]), ' ');

		int graph_index;
		if (i == 0)
		{
			Node n_src;
			graph_index = add_node(n_src);
		} 
		else
		{
			std::vector<std::string> xid_elems = split(elems[0], ':');
			Node n(xid_elems[0], xid_elems[1]);
			graph_index = add_node(n);
		}
		dag_idx_to_graph_idx.push_back(graph_index);
	}
	
	// Pass 2: add edges
	for (int i = 0; i < lines.size(); i++)
	{
		std::vector<std::string> elems = split(trim(lines[i]), ' ');

		for (int j = 1; j < elems.size(); j++)
		{
			int dag_idx = stoi(elems[j], 0, 10);
			add_edge(dag_idx_to_graph_idx[i], dag_idx_to_graph_idx[dag_idx+1]);
		}
	}
}
void
Graph::construct_from_re_string(std::string re_string)
{
	// split on ' '
	std::vector<std::string> components = split(re_string, ' ');

	// keep track of the last "intent" node and last fallback node 
	// we added to the dag
	Node n_src;
	std::size_t last_intent_idx = add_node(n_src);
	std::size_t last_fallback_idx;
	std::size_t first_fallback_idx;

	// keep track of whether or not we're currently processing a fallback path
	bool processing_fallback = false;
	bool just_processed_fallback = false;
	bool just_started_fallback = false;

	// Process each component one at a time. If it's a '(' or a ')', start or
	// stop fallback processing respectively
	for (int i = 1; i < components.size(); i++)
	{
		if ( components[i] == "(")
		{
			processing_fallback = true;
			just_started_fallback = true;
		}
		else if (components[i] == ")")
		{
			processing_fallback = false;
			just_processed_fallback = true;
		}
		else
		{
			// TODO: verify that this component is a valid XID
			std::vector<std::string> xid_elems = split(components[i], ':');
			Node n(xid_elems[0], xid_elems[1]);
			std::size_t cur_idx = add_node(n);

			if (processing_fallback)
			{
				if (just_started_fallback)
				{
					first_fallback_idx = cur_idx;
					just_started_fallback = false;
				}
				else
				{
					add_edge(last_fallback_idx, cur_idx);
				}

				last_fallback_idx = cur_idx;
			}
			else
			{
				add_edge(last_intent_idx, cur_idx);

				if (just_processed_fallback)
				{
					add_edge(last_intent_idx, first_fallback_idx);
					add_edge(last_fallback_idx, cur_idx);
					just_processed_fallback = false;
				}

				last_intent_idx = cur_idx;
			}
		}
	}
}

/**
* @brief Fill a sockaddr_x with this DAG
*
* Fill the provided sockaddr_x with this DAG
*
* @param s The sockaddr_x struct to be filled in (allocated by caller)
*/
void 
Graph::fill_sockaddr(sockaddr_x *s) const
{
	s->sx_family = AF_XIA;
	s->sx_addr.s_count = num_nodes();

	for (int i = 0; i < num_nodes(); i++)                   
	{                                                            
		node_t* node = (node_t*)&(s->sx_addr.s_addr[i]); // check this

	    // Set the node's XID and type                           
		node->s_xid.s_type = get_node(i).type();
	    memcpy(&(node->s_xid.s_id), get_node(i).id(), Node::ID_LEN);   
	                                                             
	    // Get the node's out edge list                          
	    std::vector<std::size_t> out_edges;                      
	    if (i == num_nodes()-1) {                           
	        out_edges = get_out_edges(-1); // put the source node's edges here
	    } else {                                                 
	        out_edges = get_out_edges(i);                   
	    }                                                        
	                                                             
	    // Set the out edges in the header                       
	    for (uint8_t j = 0; j < EDGES_MAX; j++)              
	    {                                                        
	        if (j < out_edges.size())                            
	            node->s_edge[j] = out_edges[j];                   
	        else                                                 
	            node->s_edge[j] = EDGE_UNUSED;
	    }                                                        
	}
}


/**
* @brief Fills an empty graph from a sockaddr_x.
*
* Fills an empty graph from a sockaddr_x. Behavior is undefined if the graph
* contains nodes or edges prior to this call.
*
* @param s The sockaddr_x.
*/
void
Graph::from_sockaddr(const sockaddr_x *s)
{
	int num_nodes = s->sx_addr.s_count;
	// First add nodes to the graph and remember their new indices
	std::vector<uint8_t> graph_indices;
	for (int i = 0; i < num_nodes; i++)
	{
		const node_t *node = &(s->sx_addr.s_addr[i]);
		Node n = Node(node->s_xid.s_type, &(node->s_xid.s_id), 0); // 0 means nothing
		graph_indices.push_back(add_node(n));
	}

	// Add the source node
	uint8_t src_index = add_node(Node());

	// Add edges
	for (int i = 0; i < num_nodes; i++)
	{
		const node_t *node = &(s->sx_addr.s_addr[i]);

		int from_node;
		if (i == num_nodes-1)
			from_node = src_index;
		else
			from_node = graph_indices[i];

		for (int j = 0; j < EDGES_MAX; j++)
		{
			int to_node = node->s_edge[j];
			
			if (to_node != EDGE_UNUSED)
				add_edge(from_node, to_node);
		}
	}
}


/**
* @brief Replace the DAG's final intent with the supplied Node.
*
* Replace the DAG's final intent with the supplied node.
*
* @param new_intent The Node to become the DAG's new final intent.
*/
void 
Graph::replace_final_intent(const Node& new_intent)
{
	std::size_t intent_index = final_intent_index();
	nodes_[intent_index] = new_intent;
}

/**
* @brief Return the final intent of the DAG.
*
* Returns the DAG's final intent (as a Node).
*
* @return the DAG's final intent
*/
Node
Graph::get_final_intent() const
{
	std::size_t intent_index = final_intent_index();
	return nodes_[intent_index];
}

