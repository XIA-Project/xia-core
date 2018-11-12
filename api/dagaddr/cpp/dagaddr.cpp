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
 @brief Implements the dagaddr classes Node and Graph
*/


#include "dagaddr.hpp"
/*! \cond */
#include "utils.hpp"
#include <cstring>
#include <map>
#include <stack>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h> // for uint8_t
#include <execinfo.h> // for backtrace, backtrace_symbols_fd
/*! \endcond */

static const std::size_t vector_find_npos = std::size_t(-1);

/*
 * load user defined XIDs for use in parsing and unparsing DAGs
 */
Node::XidMap Node::load_xids()
{
	Node::XidMap ids;

	char path[PATH_SIZE];
	char name[256], text[256];
	short id;
	unsigned len = sizeof(path);
	char *p;

	if ((p = getenv("XIADIR")) != NULL) {
		strncpy(path, p, len);
	} else if (!getcwd(path, len)) {
		path[0] = 0;
	}

	p = strstr(path, SOURCE_DIR);
	if (p) {
		p += sizeof(SOURCE_DIR) - 1;
		*p = '\0';
	}
	strncat(path, "/etc/xids", len);

	FILE *f = fopen(path, "r");
	if (f) {
		while (!feof(f)) {
			if (fgets(text, sizeof(text), f)) {
				if (sscanf(text, "%hi %s", &id, name) == 2) {
					ids[id] = name;
				}
			}
		}
		fclose(f);
	}

	return ids;
}

// call the load_xids function at init time to fill in the hash map
Node::XidMap Node::xids = Node::load_xids();



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
const std::string Node::XID_TYPE_NCID_STRING = "NCID";
const std::string Node::XID_TYPE_SID_STRING = "SID";
const std::string Node::XID_TYPE_IP_STRING = "IP";
const std::string Node::XID_TYPE_FID_STRING = "FID";


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
	(void)dummy;
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
*			\n Node::XID_TYPE_NCID (NamedContent)
*			\n Node::XID_TYPE_SID (Service)
*			\n Node::XID_TYPE_FID (Flood)
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
		const uint8_t byte = (uint8_t) stoi(id_str.substr(2*i, 2), 0, 16);
		memcpy(&(ptr_->id[i]), &byte, 1);
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
*			\n Node::XID_TYPE_NCID_STRING
*			\n Node::XID_TYPE_SID_STRING
*			\n Node::XID_TYPE_FID_STRING
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
	++(ptr_->ref_count);
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
	memset(ptr_->id,0,Node::ID_LEN); // zero the ID

	std::string typestr = type_str;

	// Convert type_str to uppercase for string comparison
	transform(typestr.begin(), typestr.end(), typestr.begin(), ::toupper);

	if (typestr == XID_TYPE_AD_STRING)
		ptr_->type = XID_TYPE_AD;
	else if (typestr == XID_TYPE_HID_STRING)
		ptr_->type = XID_TYPE_HID;
	else if (typestr == XID_TYPE_CID_STRING)
		ptr_->type = XID_TYPE_CID;
	else if (typestr == XID_TYPE_NCID_STRING)
		ptr_->type = XID_TYPE_NCID;
	else if (typestr == XID_TYPE_SID_STRING)
		ptr_->type = XID_TYPE_SID;
	else if (typestr == XID_TYPE_IP_STRING)
		ptr_->type = XID_TYPE_IP;
	else if (typestr == XID_TYPE_FID_STRING)
		ptr_->type = XID_TYPE_FID;
	else if (typestr == XID_TYPE_DUMMY_SOURCE_STRING)
		ptr_->type = XID_TYPE_DUMMY_SOURCE;
	else
	{
		// see if it's a user defined XID
		XidMap::const_iterator itr;
		int found = 0;

		for (itr = Node::xids.begin(); itr != Node::xids.end(); itr++) {

			if (typestr == (*itr).second) {
				found = 1;
				ptr_->type = (*itr).first;
				break;
			}
		}

		if (!found) {
			ptr_->type = 0;
			printf("WARNING: Unrecognized XID type: %s\n", typestr.c_str());
			delete ptr_;
			throw std::range_error("Creating Node from unknown XID");
		}
	}

	// If this is a 4ID formatted as an IP address (x.x.x.x), we
	// need to handle it separately. Otherwise, treat string as
	// hex digits.
	uint32_t ip = inet_addr(id_str.c_str());
	if (ptr_->type == XID_TYPE_IP && ip != INADDR_NONE)
	{
		ptr_->id[0] = 0x45;  // set some special "4ID" values
		ptr_->id[5] = 0x01;
		ptr_->id[8] = 0xFA;
		ptr_->id[9] = 0xFA;

		ptr_->id[16] = *(((unsigned char*)&ip)+0);
		ptr_->id[17] = *(((unsigned char*)&ip)+1);
		ptr_->id[18] = *(((unsigned char*)&ip)+2);
		ptr_->id[19] = *(((unsigned char*)&ip)+3);
	}
	else
	{
		if (id_str.length() != 40)
		{
			printf("WARNING: XID string must be 40 characters (20 hex digits): %s\n", id_str.c_str());
			delete ptr_;
			throw std::range_error("XID string must be 40 characters");
		}
		for (std::size_t i = 0; i < ID_LEN; i++)
		{
			int num = stoi(id_str.substr(2*i, 2), 0, 16);
			if (num == -1)
			{
				printf("WARNING: Error parsing XID string (should be 20 hex digits): %s\n", id_str.c_str());
				delete ptr_;
				throw std::range_error("Error parsing XID string");
			}
			else {
				const uint8_t byte = (uint8_t) num;
				memcpy(&(ptr_->id[i]), &byte, 1);
			}
		}
	}
}

/**
* @brief Return the node's type as a string
*
* The type string will either be one of the following built-in XID types or will match
* one of the entries in etc/xids.
*			\n Node::XID_TYPE_AD_STRING
*			\n Node::XID_TYPE_HID_STRING
*			\n Node::XID_TYPE_CID_STRING
*			\n Node::XID_TYPE_SID_STRING
*			\n Node::XID_TYPE_FID_STRING
*			\n Node::XID_TYPE_IP_STRING
*			\n Node::XID_TYPE_DUMMY_SOURCE
*			\n Node:: ID_TYPE_UNKNOWN_STRING
* @return the node's type as a string
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
		case XID_TYPE_NCID:
			return XID_TYPE_NCID_STRING;
		case XID_TYPE_SID:
			return XID_TYPE_SID_STRING;
		case XID_TYPE_FID:
			return XID_TYPE_FID_STRING;
		case XID_TYPE_IP:
			return XID_TYPE_IP_STRING;
		case XID_TYPE_DUMMY_SOURCE:
			return XID_TYPE_DUMMY_SOURCE_STRING;
		default:
			std::string s = xids[this->type()];
			if (s.empty())
				s = XID_TYPE_UNKNOWN_STRING;

			return s;
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
* @brief Return a string representing the node.
*
* @return A string of the form <type>:<id>
*/
std::string
Node::to_string() const
{
	return type_string() + ":" + id_string();
}


/**
 * @brief Check if the XID represented by this node is valid
 *
 * @return true if node has a valid XID, false otherwise
 */
bool
Node::has_valid_xid() const
{
	bool valid = false;

	// Check that the type is not dummy
	switch (this->type()) {
		case XID_TYPE_UNKNOWN:
		case XID_TYPE_DUMMY_SOURCE:
			valid = false;
			break;
		case XID_TYPE_AD:
		case XID_TYPE_IP:
		case XID_TYPE_CID:
		case XID_TYPE_NCID:
		case XID_TYPE_FID:
		case XID_TYPE_HID:
		case XID_TYPE_SID:
			valid = true;
			break;
		default:
			std::string s = xids[this->type()];
			if (!s.empty()) {
				valid = true;
			}
	}

	return valid;
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
	if (dag_string.find("http://") != std::string::npos)
	{
		construct_from_http_url_string(dag_string);
	}
	else if (dag_string.find("DAG") != std::string::npos)
	{
		construct_from_dag_string(dag_string);
	}
	else if (dag_string.find("RE") != std::string::npos)
	{
		construct_from_re_string(dag_string);
	}
	else
	{
		throw std::range_error("Improperly formatted string");
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

void
Graph::append_node_str(std::string node_str)
{
	Node node(node_str);
	*this *= Graph(node);
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
			if (r.nodes_[i].type() == XID_TYPE_DUMMY_SOURCE)
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

		printf("Node %lu: [%s] ", i, nodes_[i].type_string().c_str());
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
			printf(" Node %lu", out_edges_[i][j]);
		}
		if (is_sink(i))
			printf(" [SNK]");
		printf("\n");
	}
}

void
Graph::dump_stack_trace() const
{
	void *stack_entries[50];
	size_t size;

	size = backtrace(stack_entries, 50);
	backtrace_symbols_fd(stack_entries, size, STDERR_FILENO);
}

bool
Graph::remove_intent_sid_node()
{
	// Ensure that the sink node is an SID
	std::size_t intent_index = final_intent_index();
	if (nodes_[intent_index].type() != XID_TYPE_SID) {
		return false;
	}
	return remove_intent_node();
}

bool
Graph::remove_intent_node()
{
	std::size_t intent_index = final_intent_index();

	// If there's just one node in graph, return error
	if (num_nodes() <= 1) {
		printf("Graph::remove_intent_node() removal would empty the graph\n");
		return false;
	}

	// Ensure that there's just one incoming edge to the intent node
	if (in_edges_[intent_index].size() != 1) {
		printf("Graph::remove_intent_node() must have 1 incoming edge\n");
		return false;
	}

	// Remove the incoming edge to intent node
	for (size_t i=0; i<nodes_.size(); i++) {
		std::vector<std::size_t>::iterator it;
		for(it=out_edges_[i].begin(); it!=out_edges_[i].end(); it++) {
			if (*it == intent_index) {
				out_edges_[i].erase(it);
				break;
			}
		}
	}

	// Remove the intent node
	nodes_.erase(nodes_.begin() + intent_index);
	in_edges_.erase(in_edges_.begin() + intent_index);
	out_edges_.erase(out_edges_.begin() + intent_index);
	return true;
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

// @brief Remove an edge entry from the given vector
// @return true if edge removed successfully
// @return false if edge to be removed was not found in the vector
bool
Graph::remove_vector_edge(std::vector<std::size_t> &vec, std::size_t id)
{
	bool retval = false;
	for(auto it=vec.begin(); it!=vec.end();) {
		if(*it == id) {
			it = vec.erase(it);
			retval = true;
		} else {
			++it;
		}
	}
	return retval;
}

// @brief Remove an edge
// @returns true if edge removed from both internal vectors successfully
// @returns false if an edge to be removed was not found.
//
bool
Graph::remove_edge(std::size_t from_id, std::size_t to_id)
{
	if(from_id == to_id) {
		return true;
	}
	// Walk out_edges_[from_id] list and remove the edge
	if(remove_vector_edge(out_edges_[from_id], to_id) == false) {
		return false;
	}
	if(remove_vector_edge(in_edges_[to_id], from_id) == false) {
		return false;
	}
	return true;
}

/**
 * brief Remove primary edge to first SID in a two SID graph
 *
 * For a graph like *-> AD -> HID --------> SID1
 *                             '--> SID2 --'
 * The edge between HID and SID1 is removed, so it becomes
 *                  *-> AD -> HID -> SID2 -> SID1
 * @returns true on success and false on failure
 */
bool
Graph::flatten_double_sid()
{
	int sid_count = 0;
	// First walk the list of nodes to make sure there are two SIDs
	for (auto it=nodes_.cbegin(); it!=nodes_.cend(); it++) {
		if((*it).type() == XID_TYPE_SID) {
			sid_count++;
		}
	}
	if(sid_count != 2) {
		printf("Graph::flatten_double_sid expected 2 got %d\n", sid_count);
		return false;
	}
	std::size_t intent_hid_index = intent_HID_index();
	std::size_t intent_sid_index = intent_SID_index();
	if(remove_edge(intent_hid_index, intent_sid_index) == false) {
		return false;
	}
	return true;
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

	// Walk through the other graph's nodes
	for (std::vector<Node>::const_iterator it = r.nodes_.begin(); it != r.nodes_.end(); ++it) {
		if (allow_duplicate_nodes && (*it).type() == XID_TYPE_DUMMY_SOURCE) {
			// don't add r's source node to the middle of this graph
			node_mapping.push_back(-1);
		} else {
			// Add all of other graph's nodes to this one (skip dups if needed)
			// Store the newly added nodes' indexes in node_mapping
			// Use indexes of existing nodes when duplicates are seen
			node_mapping.push_back(add_node(*it, allow_duplicate_nodes));
		}
	}

	// Now walk the nodes in the other graph
	for (std::size_t from_id = 0; from_id < r.out_edges_.size(); from_id++) {
		if (allow_duplicate_nodes) {
		   if(r.nodes_[from_id].type() == XID_TYPE_DUMMY_SOURCE) {
			   continue;
		   }
		}
		// Out edges for each node in the other graph
		for (std::vector<std::size_t>::const_iterator it = r.out_edges_[from_id].begin(); it != r.out_edges_[from_id].end(); ++it) {
			add_edge(node_mapping[from_id], node_mapping[*it]);
		}
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
		idx_str = (char*)malloc(sizeof(char) * size +1); // +1 for null char (sprintf automatically appends it)
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
Graph::index_from_dag_string_index(int32_t dag_string_index, std::size_t source_index, std::size_t sink_index) const
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

std::size_t
Graph::intent_XID_index(uint32_t xid_type) const
{
	std::size_t intentIndex = INVALID_GRAPH_INDEX;

	std::size_t curIndex;
	std::size_t source = source_index();
	std::size_t intent = final_intent_index();
	std::stack<std::size_t> dfstack;

	// Start walking depth first from source to sink
	dfstack.push(source);

	while (!dfstack.empty()) {

		// the last seen node
		curIndex = dfstack.top();
		dfstack.pop();

		// save it's outgoing edges, in reverse, with first on top
		std::vector<std::size_t> edges = out_edges_[curIndex];
		std::vector<std::size_t>::reverse_iterator rit;
		for(rit = edges.rbegin(); rit != edges.rend(); rit++) {
			dfstack.push(*rit);
		}

		// Save last node matching xid_type as intent XID
		if (nodes_[curIndex].type() == xid_type) {
			intentIndex = curIndex;
		}

		// If intent XID is known on reaching intent node, we have a result
		if (curIndex == intent && intentIndex != INVALID_GRAPH_INDEX) {

			// Return the result to caller
			// TODO: If called too much, this function may be expensive
			/*
			printf("Graph::intent_XID_index: found index:%zu\n", intentIndex);
			printf("for: %s\n", this->dag_string().c_str());
			dump_stack_trace();
			*/
			break;
		}

	}

	return intentIndex;

	/* This is a lighter weight implementation that just walks first hops
	 * TODO: Consider using this if this function is too expensive
	// Build first_path by walking first hops from source to intent node
	for(curIndex=source; curIndex!=intent; curIndex=out_edges_[curIndex][0]) {

		// Find last node matching xid_type while walking to intent node
		if (nodes_[curIndex].type() == xid_type) {
			intentIndex = curIndex;
		}
	}

	// Check if the intent node is the one we are looking for
	if (nodes_[curIndex].type() == xid_type) {
		intentIndex = curIndex;
	}

	// TODO: Remove this debug print
	if (intentIndex != INVALID_GRAPH_INDEX) {
		printf("Graph::intent_XID_index: found index:%zu\n", curIndex);
	}

	return intentIndex;
	*/
}

std::size_t
Graph::intent_CID_index() const
{
	return intent_XID_index(XID_TYPE_CID);
}

std::size_t
Graph::intent_SID_index() const
{
	return intent_XID_index(XID_TYPE_SID);
}

std::size_t
Graph::intent_HID_index() const
{
	return intent_XID_index(XID_TYPE_HID);
}

std::size_t
Graph::intent_AD_index() const
{
	return intent_XID_index(XID_TYPE_AD);
}

bool
Graph::replace_intent_XID(uint32_t xid_type, std::string new_xid_str)
{
	std::size_t intent_xid_index = intent_XID_index(xid_type);
	if (intent_xid_index == INVALID_GRAPH_INDEX) {
		return false;
	}
	Node new_xid(new_xid_str);
	nodes_[intent_xid_index] = new_xid;
	return true;
}

bool
Graph::replace_intent_HID(std::string new_hid_str)
{
	return replace_intent_XID(XID_TYPE_HID, new_hid_str);
}

bool
Graph::replace_intent_AD(std::string new_ad_str)
{
	return replace_intent_XID(XID_TYPE_AD, new_ad_str);
}

/**
  * @brief Compare two graphs while ignoring intent AD
  *
  * @return 0 if the graphs are same except intent AD, -1 otherwise
  */
int
Graph::compare_except_intent_AD(Graph other) const
{
	// Copy the other graph so we can modify the copy
	Graph them(other);

	// Find our and their intent AD
	size_t intent_ad = intent_AD_index();
	if (intent_ad == INVALID_GRAPH_INDEX) {
		//printf("Graph::compare_except_intent_AD No intent AD\n");
		return -1;
	}
	size_t their_intent_ad = them.intent_AD_index();
	if (their_intent_ad == INVALID_GRAPH_INDEX) {
		//printf("Graph::compare_except_intent_AD No other intent AD\n");
		return -1;
	}

	// Replace their intent AD with ours
	them.nodes_[their_intent_ad] = nodes_[intent_ad];

	// Compare them with us
	Graph us(*this);
	if (us == them) {
		return 0;
	}

	//printf("Graph::compare_except_intent_AD: ERROR mismatched graphs\n");
	//printf("this: %s\n", this->dag_string().c_str());
	//printf("them: %s\n", them.dag_string().c_str());
	return -1;
}

/**
 * @brief add given SID as a fallback to the intent SID
 *
 * For a Graph like AD -> HID -> SID
 * this function adds a fallback SID from HID.
 *
 * @return 0 on success, -1 on failure
 */
int
Graph::add_sid_fallback(std::string sid)
{
	// TODO: check to make sure intent is an SID
	int intent_index = final_intent_index();
	if(intent_index == -1) {
		printf("Graph::add_sid_fallback: ERROR finding intent node\n");
		return -1;
	}
	size_t hid_index = intent_HID_index();
	if (hid_index == INVALID_GRAPH_INDEX) {
		printf("Graph::intent_HID_str: HID index not found\n");
		return -1;
	}
	// Now add a node to this graph
	int idx = add_node(sid, false);
	add_edge(hid_index, idx);
	add_edge(idx, final_intent_index());

	return 0;
}

/**
 * @brief Return the AD for the Graph's intent node
 *
 * Get the AD string on first path to intent node
 * Note: This function is essentially identical to intent_HID_str()
 *
 * @return The AD if found, empty string otherwise
 */
std::string
Graph::intent_AD_str() const
{
	std::string ad;
	std::size_t ad_index = intent_AD_index();
	if (ad_index == INVALID_GRAPH_INDEX) {
		return "";
	}
	return nodes_[ad_index].to_string();
}

const Node&
Graph::intent_AD() const
{
	std::size_t ad_index = intent_AD_index();
	if (ad_index == INVALID_GRAPH_INDEX) {
		printf("Graph::intent_AD: AD index not found\n");
		throw std::range_error("AD not found");
	}
	return nodes_[ad_index];
}

/*
 * @brief Return the HID for the Graph's intent node
 *
 * Get the HID string on first path to intent node
 * Note: This function is essentially identical to intent_AD_str()
 *
 * @return The HID if found, empty string otherwise
 */
std::string
Graph::intent_HID_str() const
{
	std::string hid;
	std::size_t hid_index = intent_HID_index();
	if (hid_index == INVALID_GRAPH_INDEX) {
		printf("Graph::intent_HID_str: HID index not found\n");
		return "";
	}
	return nodes_[hid_index].to_string();
}

const Node&
Graph::intent_HID() const
{
	std::size_t hid_index = intent_HID_index();
	if (hid_index == INVALID_GRAPH_INDEX) {
		printf("Graph::intent_HID: HID index not found\n");
		throw std::range_error("HID not found");
	}
	return nodes_[hid_index];
}

/*
 * @brief Return the SID for the Graph's intent node
 *
 * Get the SID string on first path to intent node
 * Note: This function is essentially identical to intent_AD_str()
 *
 * @return The SID if found, empty string otherwise
 */
std::string
Graph::intent_SID_str() const
{
	std::string sid;
	std::size_t sid_index = intent_SID_index();
	if (sid_index == INVALID_GRAPH_INDEX) {
		return "";
	}
	return nodes_[sid_index].to_string();
}

const Node&
Graph::intent_SID() const
{
	std::size_t sid_index = intent_SID_index();
	if (sid_index == INVALID_GRAPH_INDEX) {
		printf("Graph::intent_SID: SID index not found\n");
		throw std::range_error("SID not found");
	}
	return nodes_[sid_index];
}

/*
 * @brief Return the CID for the Graph's intent node
 *
 * Get the CID string on first path to intent node
 * Note: This function is essentially identical to intent_AD_str()
 *
 * @return The CID if found, empty string otherwise
 */
std::string
Graph::intent_CID_str() const
{
	std::string cid;
	std::size_t cid_index = intent_CID_index();
	if (cid_index == INVALID_GRAPH_INDEX) {
		return "";
	}
	return nodes_[cid_index].to_string();
}

const Node&
Graph::intent_CID() const
{
	std::size_t cid_index = intent_CID_index();
	if (cid_index == INVALID_GRAPH_INDEX) {
		printf("Graph::intent_CID: CID index not found\n");
		throw std::range_error("CID not found");
	}
	return nodes_[cid_index];
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
	std::size_t sink_index = -1, source_index = -1;

	// Find source and sink
	bool found_source = false, found_sink = false;
	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (is_source(i)) {
			source_index = i;
			found_source = true;
		}

		if (is_sink(i)) {
			sink_index = i;
			found_sink = true;
		}
	}

	if (found_source && found_sink)
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

std::string
Graph::http_url_string() const{
	std::string dag_string = this->dag_string();
	dag_string.erase(std::remove(dag_string.begin(), dag_string.end(), '\n'), dag_string.end());
	std::replace(dag_string.begin(), dag_string.end(), ' ', '.');
	std::replace(dag_string.begin(), dag_string.end(), ':', '$');
	return "http://" + dag_string;
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
		if (is_sink(i)) {
			if (i != source_index()) {
				return i;
			}
		}
	}

	return -1;
}

/**
* @brief Return XID as a string for node at specified index
*
* Get the XID in string format from nodes_ at specified index.
*
* @return XID string
*/
std::string
Graph::xid_str_from_index(std::size_t node) const
{
	if (node >= nodes_.size()) {
		printf("Error: Graph::xid_str_from_index(): invalid index\n");
		return "";
	}
	return nodes_[node].to_string();
}

/**
 * @brief Check if this is a valid graph
 *
 * Make sure this Graph represents a valid XIA DAG. For now we just do
 * a bunch of simple checks like making sure there's at least one node
 * and that the intent node has a valid XID type.
 *
 * @return true if the graph is valid, false otherwise
 */

bool
Graph::is_valid() const
{
	// A series of checks to make sure this is a valid graph

	// Should have at least one node
	if (num_nodes() < 1) {
		return false;
	}

	// Ensure that the Node at intent index has a valid XID type
	std::size_t intent = final_intent_index();
	Node intent_node = nodes_[intent];
	if (!intent_node.has_valid_xid()) {
		return false;
	}

	// All checked out fine
	return true;
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

	printf("Warning: is_final_intent: node not found in DAG: %s\n",
			n.id_string().c_str());
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

	printf("Warning: is_final_intent: node not found in DAG: %s\n",
			xid_str.c_str());
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
	std::size_t nIndex;
	std::size_t curIndex = source_index();
	bool found = false;
	while (!found) {
		if (nodes_[curIndex] == n) {
			nIndex = curIndex;
			found = true;
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
	while (intentIndex == nIndex ||
			(nodes_[intentIndex].type() != XID_TYPE_CID
			 && nodes_[intentIndex].type() != XID_TYPE_NCID
			 && nodes_[intentIndex].type() != XID_TYPE_SID))
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
		for (std::size_t i = 0; i < out_edges_[curIndex].size(); i++)
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

		for (std::size_t i = 0; i < out_edges_[old_index].size(); i++)
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
	std::size_t src_index = -1, sink_index = -1;
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
	std::size_t src_index = -1, sink_index = -1;
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
Graph::construct_from_http_url_string(std::string url_string){
	std::string dag_string = url_string.substr(7, std::string::npos);
	std::replace(dag_string.begin(), dag_string.end(), '.', ' ');
	std::replace(dag_string.begin(), dag_string.end(), '$', ':');
	this->construct_from_dag_string(dag_string);
}


void
Graph::construct_from_dag_string(std::string dag_string)
{
	// Check for malformed DAG string
	if (check_dag_string(dag_string) == -1)
	{
		printf("WARNING: DAG string is malformed: %s\n", dag_string.c_str());
		throw std::range_error("Invalid DAG string");
	}

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
	for (std::size_t i = 0; i < lines.size(); i++)
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
			//std::vector<std::string> xid_elems = split(elems[0], ':');
			Node n(elems[0]);
			graph_index = add_node(n);
		}
		dag_idx_to_graph_idx.push_back(graph_index);
	}

	// Pass 2: add edges
	for (std::size_t i = 0; i < lines.size(); i++)
	{
		std::vector<std::string> elems = split(trim(lines[i]), ' ');

		for (std::size_t j = 1; j < elems.size(); j++)
		{
			int dag_idx = stoi(elems[j], 0, 10);
			add_edge(dag_idx_to_graph_idx[i], dag_idx_to_graph_idx[dag_idx+1]);
		}
	}
}

/**
* @brief Checks that a DAG string is formatted properly.
*
* Runs some simple checks on the supplied DAG string to make sure it's not
* malformed.
*
* @param dag_string The DAG string to check
*
* @return 1 if string is OK, -1 otherwise
*/
int
Graph::check_dag_string(std::string dag_string)
{
	// TODO: Implement
	(void)dag_string;
	return 1;
}

void
Graph::construct_from_re_string(std::string re_string)
{
	// Check for malformed RE string
	if (check_re_string(re_string) == -1)
	{
		printf("WARNING: RE string is malformed: %s\n", re_string.c_str());
		throw std::range_error("Malformed RE string");
	}

	// split on ' '
	std::vector<std::string> components = split(re_string, ' ');

	// keep track of the last "intent" node and last fallback node
	// we added to the dag
	Node n_src;
	std::size_t last_intent_idx = add_node(n_src);
	std::size_t last_fallback_idx = -1, first_fallback_idx = -1;

	// keep track of whether or not we're currently processing a fallback path
	bool processing_fallback = false;
	bool just_processed_fallback = false;
	bool just_started_fallback = false;

	// Process each component one at a time. If it's a '(' or a ')', start or
	// stop fallback processing respectively
	for (std::size_t i = 1; i < components.size(); i++)
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
			//std::vector<std::string> xid_elems = split(components[i], ':');
			Node n(components[i]);
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
* @brief Checks that an RE string is formatted properly.
*
* Runs some simple checks on the supplied RE string to make sure it's not
* malformed.
*
* @param re_string The RE string to check
*
* @return 1 if string is OK, -1 otherwise
*/
int
Graph::check_re_string(std::string re_string)
{
	// TODO: Implement
	(void)re_string;
	return 1;
}

/**
* @brief Fill a wire buffer with this DAG
*
* Fill the provided buffer with this DAG
*
* @param s The wire buffer struct to be filled in (allocated by caller)
*/
size_t
Graph::fill_wire_buffer(node_t *buf) const
{
	for (int i = 0; i < num_nodes(); i++)
	{
		node_t* node = buf + i; // check this

		// Set the node's XID and type
		node->xid.type = htonl(get_node(i).type());
		memcpy(&(node->xid.id), get_node(i).id(), Node::ID_LEN);

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
				node->edge[j].idx = out_edges[j];
			else
				node->edge[j].idx = EDGE_UNUSED;

			// On creation, none of the edges have been visited
			node->edge[j].visited = 0;
		}
	}
	return num_nodes();
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
#ifdef __APPLE__
	// the length field is not big enough for the size of a sockaddr_x
	// we don't use it anywhere in our code, so just set it to a known state.
	s->sx_len = 0;
#endif
	s->sx_addr.s_count = num_nodes();

	(void) fill_wire_buffer(s->sx_addr.s_addr);
}

/**
 * @ Fills an empty graph from our binary wire format
 *
 * Fills an empty graph from a memory buffer containing the graph in
 * serialized wire form.
 *
 * @param num_nodes The number of nodes in the graph
 * @param buf The memory buffer containing graph in serialized wire format
 */
void
Graph::from_wire_format(uint8_t num_nodes, const node_t *buf)
{
	// A graph cannot have more than CLICK_XIA_ADDR_MAX_NODES
	if (num_nodes > CLICK_XIA_ADDR_MAX_NODES) {
		printf("Graph::from_wire_format ERROR: num_nodes: %d\n", num_nodes);
		throw std::range_error("too many nodes in wire format");
	}

	// First add nodes to the graph and remember their new indices
	std::vector<uint8_t> graph_indices;
	for (int i = 0; i < num_nodes; i++)
	{
		const node_t *node = &(buf[i]);
		Node n = Node(ntohl(node->xid.type), &(node->xid.id), 0); // 0=dummy
		graph_indices.push_back(add_node(n));
	}

	// Add the source node
	uint8_t src_index = add_node(Node());

	// Add edges
	for (int i = 0; i < num_nodes; i++)
	{
		const node_t *node = &(buf[i]);

		int from_node;
		if (i == num_nodes-1) {
			from_node = src_index;
		} else {
			from_node = graph_indices[i];
		}

		if (from_node > CLICK_XIA_ADDR_MAX_NODES) {
			printf("Graph::from_wire_format ERROR from_node: %d\n",from_node);
			throw std::range_error("invalid from_node in wire format");
		}

		for (int j = 0; j < EDGES_MAX; j++)
		{
			int to_node = node->edge[j].idx;

			if(to_node > CLICK_XIA_ADDR_MAX_NODES &&
					to_node != CLICK_XIA_XID_EDGE_UNUSED) {
				printf("Graph::from_wire_format ERROR to_node:%d\n",to_node);
				throw std::range_error("invalid to_node in wire format");
			}

			if (to_node != EDGE_UNUSED) {
				add_edge(from_node, to_node);
			}
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
	if(s->sx_family != AF_XIA) {
		printf("Graph::from_sockaddr: Error: sockaddr_x family is not XIA\n");
		throw std::range_error("sockaddr_x family is not XIA");
	}

	uint8_t num_nodes = s->sx_addr.s_count;
	from_wire_format(num_nodes, s->sx_addr.s_addr);
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
* @brief Replace the DAG's final intent with the supplied Node.
*
* Replace the DAG's final intent with the supplied node.
*
* @param new_intent The Node to become the DAG's new final intent.
*/
void
Graph::replace_node_at(int i, const Node& new_node)
{
	// FIXME: validate that i is a valid index into the dag
	std::size_t src_index = -1, sink_index = -1;
	for (std::size_t j = 0; j < nodes_.size(); j++)
	{
		if (is_source(j)) src_index = j;
		if (is_sink(j)) sink_index = j;
	}

	nodes_[index_from_dag_string_index(i, src_index, sink_index)] = new_node;
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


/**
* @brief Get a list of nodes of the specified type.
*
* Return a list of the nodes in the DAG of the specified XID type. The vector
* returned contains pointers to the node objects in the DAG, not copies.
*
* @param xid_type The XID type of interest. Must be one of:
* 			\n Node::XID_TYPE_AD (Administrative Domain)
*			\n Node::XID_TYPE_HID (Host)
*			\n Node::XID_TYPE_CID (Content)
*			\n Node::XID_TYPE_SID (Service)
*			\n Node::XID_TYPE_FID (Flood)
*			\n Node::XID_TYPE_IP (IPv4 / 4ID)
*
* @return List of pointers to nodes of the specified type.
*/
std::vector<const Node*>
Graph::get_nodes_of_type(unsigned int type) const
{
	std::vector<const Node*> nodes;
	std::vector<Node>::const_iterator it;
	for (it = nodes_.begin(); it != nodes_.end(); ++it) {
		if (it->type() == type) {
			nodes.push_back(&*it);
//			printf("FOUND IT");
		}
	}

	return nodes;
}

/**
* @brief Flatten the direct edge from source to intent, if it exists
*
* Try to flatten a graph with a direct edge between the source and intent
* by removing that edge.
*
* @return result of flattening the direct edge from source to intent node.
*/
bool
Graph::flatten()
{
	std::size_t source_idx = source_index();
	std::size_t sink_idx = final_intent_index();
	if (source_idx == (std::size_t)-1 || sink_idx == (std::size_t)-1) {
		return false;
	}
	if (out_edges_[source_idx].size() > 0) {
		if (out_edges_[source_idx][0] == sink_idx) {
			out_edges_[source_idx].erase(out_edges_[source_idx].begin());
			in_edges_[sink_idx].erase(in_edges_[sink_idx].begin());
		}
	}
	// if there was no edge to flatten, we still return success
	return true;
}

/**
  * @brief Recursively walk from given node to sink
  *
  * Recursively walk from given node to sink, adding each node encountered
  * to the paths vector. When called on the source node of a graph this
  * results in nodes for all possible paths from source node to sink added
  * to 'paths'. Making it possible to compare two graphs even if they
  * are not stored identically in nodes_, out_edges_ and in_edges.
  *
  * @param node index of node to start walking down from
  *
  * @param paths list of nodes seen so far, while following paths to sink
  *
  * @return false if infinite recursion is encountered, true otherwise
  *
  */
bool
Graph::depth_first_walk(int node, std::vector<Node> &stack,
		std::vector<Node> &paths) const
{
	// Break out with error if we are headed to infinite recursion
	if(paths.size() > Graph::MAX_XIDS_IN_ALL_PATHS) {
		printf("ERROR: Infinite recursion while walking Graph\n");
		return false;
	}

	// Add current node to 'paths'
	stack.push_back(get_node(node));

	// Follow all outgoing edges; sink will have none
	std::vector<std::size_t> out_edges = get_out_edges(node);
	std::vector<std::size_t>::const_iterator it;
	for(it = out_edges.begin(); it != out_edges.end(); it++) {
		// Recursively follow each outgoing edge
		if(depth_first_walk(*it, stack, paths) == false) {
			return false;
		}
	}
	// Found sink node. Stack contains path from source to sink. Save it.
	if(out_edges.size() == 0) {
		for(size_t i=0; i<stack.size(); i++) {
			paths.push_back(stack[i]);
		}
	}
	stack.pop_back();

	// We reached the sink node or all outgoing edges have been handled
	return true;
}

/**
  * @brief List of nodes in each possible path to sink, ordered by fallback
  *
  * Get a list of all nodes encountered in walking from source to sink
  * while taking all possible fallbacks.
  *
  * Example:
  *     *--> AD1 ----------------------> HID1 -> SID1
  *            \-> AD2 -> HID2 -> SID2 -/
  *
  * Result:
  *     [ * AD1 HID1 SID1 AD1 AD2 HID2 SID2 HID1 SID1 ]
  *
  * @return list of all nodes in all possible paths from source to sink
  */
bool
Graph::ordered_paths_to_sink(std::vector<Node> &paths_to_sink) const
{
	paths_to_sink.clear();
	std::vector<Node> stack;
	stack.clear();
	// Walk starting at the dummy source node
	return depth_first_walk(-1, stack, paths_to_sink);
}

/**
  * @brief Compare two graphs logically
  *
  * Two graphs can be represented differently in memory or as a string
  * We compare them logically by walking all possible paths from source
  * to sink and comparing the order of nodes on each path with those
  * in the other graph.
  *
  * @param g The other graph to compare against
  *
  * @return are the graphs logically equal?
  *
  */
bool
Graph::operator==(const Graph &g) const
{
	std::vector<Node> my_paths;
	std::vector<Node> their_paths;

	if(ordered_paths_to_sink(my_paths) == false) {
		printf("Graph::== ERROR getting my paths to sink\n");
		return false;
	}

	if(g.ordered_paths_to_sink(their_paths) == false) {
		printf("Graph::== ERROR getting their paths to sink\n");
		return false;
	}

	if(my_paths.size() != their_paths.size()) {
		return false;
	}

	std::vector<Node>::const_iterator my_it, their_it;
	my_it = my_paths.begin();
	their_it = their_paths.begin();

	for(;my_it!=my_paths.end()||their_it!=their_paths.end();my_it++,their_it++){
		if ( !((*my_it).equal_to(*their_it)) ) {
			return false;
		}
	}
	return true;
}

Graph
Graph::last_ordered_path_dag() const
{
	std::vector<Node> g_paths;
	int i;

	if(ordered_paths_to_sink(g_paths) == false) {
		printf("Graph::last_ordered_path_dag ERROR getting paths to sink\n");
		return nullptr;
	}
	/*
	printf("Graph::last_ordered_path_dag All paths:\n");
	for(i=0;i<(int)g_paths.size();i++) {
		printf("%s ", g_paths[i].to_string().c_str());
	}
	printf("\n");
	printf("Walking backwards in g_paths of size %d\n", (int)g_paths.size());
	*/
	// Walk backwards until dummy source is found
	i=g_paths.size()-1;
	for(; i>=0; i--) {
		//printf("Checking %s for dummy\n", g_paths[i].to_string().c_str());
		//printf("Type is %d\n", g_paths[i].type());
		if(g_paths[i].type() == XID_TYPE_DUMMY_SOURCE) {
			//printf("Graph::last_ordered_path_dag found dummy at %d\n",(int)i);
			Node src;
			Graph last_path_dag = src;
			for(int j=i+1;j<(int)g_paths.size();j++) {
				//printf("Adding %s to graph\n", g_paths[j].to_string().c_str());
				last_path_dag *= g_paths[j];
			}
			//printf("Graph::last_ordered_path_dag: %s\n",
					//last_path_dag.dag_string().c_str());
			return last_path_dag;
		}
	}
	/*
	// Identify the first node
	// FIXME: Assumes no fallbacks from dummy source node
	Node first_node = g_paths[0];
	printf("Graph::last_ordered_path_dag first node %s\n",
			first_node.to_string().c_str());
	printf("Graph::last_ordered_path_dag - all ordered paths:\n");
	for(int i=0; i<(int)g_paths.size(); i++) {
		printf("%s ", g_paths[i].to_string().c_str());
	}
	printf("\n");
	// Now search backwards in g_paths until we find first_node
	for(int i=g_paths.size()-1; i!=0; i--) {
		if(g_paths[i].equal_to(first_node)) {
			// Build graph from here to end of g_paths and return it
			Node dummy;
			Graph last_path_dag(dummy);
			for(int j=i; j<(int)g_paths.size(); j++) {
				last_path_dag *= Graph(g_paths[j]);
			}
			printf("%s\n", last_path_dag.dag_string().c_str());
			return last_path_dag;
		}
	}
	*/
	printf("Graph::last_ordered_path_dag ERROR building dag\n");
	return Graph();
}

int
Graph::merge_multi_host_fallback(Graph &fallback)
{
	// Merge the fallback into *this graph
	std::size_t my_intent_HID = intent_HID_index();
	std::size_t my_intent_SID = intent_SID_index();
	// Now add the fallback nodes
	std::size_t fallback_AD = add_node(fallback.intent_AD(), true);
	std::size_t fallback_HID = add_node(fallback.intent_HID(), true);
	std::size_t fallback_SID = add_node(fallback.intent_SID(), true);
	// Make the fallback connections
	add_edge(fallback_AD, fallback_HID);
	add_edge(fallback_HID, fallback_SID);
	// Connect fallback to the rest of this graph
	add_edge(my_intent_HID, fallback_AD);
	add_edge(fallback_SID, my_intent_SID);

	return 0;
}

/**
  * @brief Number of nodes in this graph
  *
  * Get the number of nodes in this graph or 0 if there are no nodes
  * Note: we may return 0 on failure
  *
  * @return number of nodes in graph minus source node
  */
size_t
Graph::unparse_node_size() const
{
	uint8_t num_nodes_ = num_nodes();
	if (num_nodes_ == (uint8_t) -1) {
		return 0;
	}
	return (size_t) num_nodes_;
}

/**
  * @brief Is the first hop from source node an SID?
  *
  * @return True if first hop from source node is SID
  *
  */
bool
Graph::first_hop_is_sid() const
{
	// Get the source node
	size_t source = source_index();
	// Then it's first hop
	std::vector<std::size_t> edges = out_edges_[source];
	size_t first_hop = edges[0];
	// Test type of first hop
	Node first_hop_node = nodes_[first_hop];
	if (first_hop_node.type() == XID_TYPE_SID) {
		return true;
	}
	return false;
}
