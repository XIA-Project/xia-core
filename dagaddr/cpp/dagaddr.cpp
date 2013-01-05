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

Node::container Node::undefined_ = {0, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 0};

const std::string Node::XID_TYPE_UNKNOWN_STRING = "UNKNOWN";
const std::string Node::XID_TYPE_AD_STRING = "AD";
const std::string Node::XID_TYPE_HID_STRING = "HID";
const std::string Node::XID_TYPE_CID_STRING = "CID";
const std::string Node::XID_TYPE_SID_STRING = "SID";
const std::string Node::XID_TYPE_IP_STRING = "IP";


/**
* @brief Create a new empty node.
*
* Create a new empty node with type XID_TYPE_UNKNOWN and id 0. This is
* 			commonly used to create the "dummy" source node.
*/
Node::Node()
	: ptr_(&undefined_)
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

/*Node::Node(uint32_t type, const void* id)
{
	ptr_ = new container;
	ptr_->type = type;
	memcpy(ptr_->id, id, ID_LEN);
	ptr_->ref_count = 1;
}*/

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

	for (int i = 0; i < ID_LEN; i++)
	{
		int num = std::stoi(id_str.substr(2*i, 2), 0, 16);
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
	else
		ptr_->type = 0;

	for (int i = 0; i < ID_LEN; i++)
	{
		int num = std::stoi(id_str.substr(2*i, 2), 0, 16);
		memcpy(&(ptr_->id[i]), &num, 1);
	}
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
	if (ptr_ == &undefined_)
		return;
	++ptr_->ref_count;
}

void
Node::release() const
{
	if (ptr_ == &undefined_)
		return;
	if (--ptr_->ref_count == 0)
	{
		delete ptr_;
		ptr_ = &undefined_;
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
*			\n Node::XID_TYPE_UNKNOWN_STRING
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
		default:
			return XID_TYPE_UNKNOWN_STRING;
	}
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
*			Example:
*<div><pre>DAG 2 0 - 
*AD:4349445f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f 2 1 - 
*HID:4849445f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f 2 - 
*SID:534944305f5f5f5f5f5f5f5f5f5f5f5f5f5f5f5f</pre></div>
*
*/
Graph::Graph(std::string dag_string)
{
	// TODO: split on -, not \n
	std::vector<std::string> lines = split(dag_string, '\n');

	// Maps indices in the graph to indices in the DAG string
	// IMPORTANT: DAG string indices are off by one since the first
	// line in a DAG string is index -1. So, when indexing into this
	// vector, ALWAYS ADD 1
	std::vector<std::size_t> dag_idx_to_graph_idx;

	// Pass 1: add nodes
	for (int i = 0; i < lines.size(); i++)
	{
		std::vector<std::string> elems = split(lines[i], ' ');

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
		std::vector<std::string> elems = split(lines[i], ' ');

		for (int j = 1; j < elems.size() - 1; j++)
		{
			int dag_idx = std::stoi(elems[j]);
			add_edge(dag_idx_to_graph_idx[i], dag_idx_to_graph_idx[dag_idx+1]);
		}
	}
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
			sources.push_back(i);

	std::vector<std::size_t> node_mapping_r;
	merge_graph(r, node_mapping_r);

	for (auto it_sink = sinks.begin(); it_sink != sinks.end(); ++it_sink)
		for (auto it_source = sources.begin(); it_source != sources.end(); ++it_source)
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

std::size_t
Graph::add_node(const Node& p)
{
	std::size_t idx = vector_push_back_unique(nodes_, p);
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

void
Graph::merge_graph(const Graph& r, std::vector<std::size_t>& node_mapping)
{
	node_mapping.clear();

	for (auto it = r.nodes_.begin(); it != r.nodes_.end(); ++it)
		node_mapping.push_back(add_node(*it));

	for (std::size_t from_id = 0; from_id < r.out_edges_.size(); from_id++)
		for (auto it = r.out_edges_[from_id].begin(); it != r.out_edges_[from_id].end(); ++it)
			add_edge(node_mapping[from_id], node_mapping[*it]);
}

std::string
Graph::out_edges_for_index(std::size_t i, std::size_t source_index, std::size_t sink_index) const
{
	std::string out_edge_string;
	for (std::size_t j = 0; j < out_edges_[i].size(); j++)
	{
		int idx = index_in_dag_string(out_edges_[i][j], source_index, sink_index);
		char *idx_str;
		int size = snprintf(NULL, 0, " %zu\0", idx);
		idx_str = (char*)malloc(sizeof(char) * size);
		sprintf(idx_str, " %zu\0", idx);
		out_edge_string += idx_str; 
		free(idx_str);
	}

	return out_edge_string;
}

std::string
Graph::xid_string_for_index(std::size_t i) const
{
	std::string xid_string;
	char *xid = (char*)malloc(sizeof(char) * 2 * Node::ID_LEN);
	for (std::size_t j = 0; j < Node::ID_LEN; j++)
		sprintf(&xid[2*j], "%02x", nodes_[i].id()[j]);
	xid_string += xid;
	free(xid);
	return xid_string;
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
	int sink_index, source_index;

	// Find source and sink
	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (is_source(i)) 
			source_index = i;

		if (is_sink(i))
			sink_index = i;
	}

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
		dag_string += xid_string_for_index(i);

		// add out edges
		dag_string += out_edges_for_index(i, source_index, sink_index);
		dag_string += " - \n";
	}

	// Add sink last
	dag_string += nodes_[sink_index].type_string() + ":";
	dag_string += xid_string_for_index(sink_index);


	return dag_string;
}
