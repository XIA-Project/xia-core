#include "dagaddr.hpp"
#include <cstring>
#include <cstdio>

Principal::container Principal::undefined_ = {0, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 0};

Principal::Principal()
	: ptr_(&undefined_)
{
}

Principal::Principal(const Principal& r)
	: ptr_(r.ptr_)
{
	acquire();
}

Principal::Principal(uint32_t type, const void* id)
{
	ptr_ = new container;
	ptr_->type = type;
	memcpy(ptr_->id, id, ID_LEN);
	ptr_->ref_count = 1;
}

Principal::~Principal()
{
	release();
}

Principal&
Principal::operator=(const Principal& r)
{
	release();
	ptr_ = r.ptr_;
	acquire();
	return *this;
}

bool
Principal::equal_to(const Principal& r) const
{
	return ptr_->type == r.ptr_->type && memcmp(ptr_->id, r.ptr_->id, ID_LEN) == 0;
}

void
Principal::acquire() const
{
	if (ptr_ == &undefined_)
		return;
	++ptr_->ref_count;
}

void
Principal::release() const
{
	if (ptr_ == &undefined_)
		return;
	if (--ptr_->ref_count == 0)
	{
		delete ptr_;
		ptr_ = &undefined_;
	}
}

Graph::Graph()
{
}

Graph::Graph(const Principal& p)
{
	add_node(p);
}

Graph::Graph(const Graph& r)
{
	nodes_ = r.nodes_;
	out_edges_ = r.out_edges_;
	in_edges_ = r.in_edges_;
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

Graph
Graph::operator+(const Graph& r) const
{
	return Graph(*this) += r;
}

void
Graph::print() const
{
	for (std::size_t i = 0; i < nodes_.size(); i++)
	{
		if (is_source(i))
			printf("[SRC] ");
		else
			printf("      ");

		printf("Node %zu: [%u] ", i, nodes_[i].type());
		//printf("%20s", nodes_[i].id());
		for (std::size_t j = 0; j < Principal::ID_LEN; j++)
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
Graph::add_node(const Principal& p)
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
