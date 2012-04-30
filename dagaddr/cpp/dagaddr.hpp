#pragma once

#include <stdint.h>	// for non-c++0x
#include <cstdint>	// for c++0x

#include <vector>

class Principal
{
public:
	static const std::size_t ID_LEN = 20;

public:
	Principal();
	Principal(const Principal& r);
	Principal(uint32_t type, const void* id);

	~Principal();

	const uint32_t& type() const { return ptr_->type; }
	const char* id() const { return ptr_->id; }

	Principal& operator=(const Principal& r);
	bool operator==(const Principal& r) const { return ptr_ == r.ptr_; }
	bool operator!=(const Principal& r) const { return ptr_ != r.ptr_; }

	bool equal_to(const Principal& r) const;

protected:
	void acquire() const;
	void release() const;

private:
	struct container
	{
		uint32_t type;
		char id[ID_LEN];
		std::size_t ref_count;
	};

	mutable container* ptr_;
	static container undefined_;
};

class Graph 
{
public:
	Graph();
	Graph(const Principal& p);
	Graph(const Graph& r);

	Graph& operator=(const Graph& r);
	Graph& operator*=(const Graph& r);
	Graph operator*(const Graph& r) const;
	Graph& operator+=(const Graph& r);
	Graph operator+(const Graph& r) const;

	void print() const;

protected:
	std::size_t add_node(const Principal& p);
	void add_edge(std::size_t from_id, std::size_t to_id);

	bool is_source(std::size_t id) const;
	bool is_sink(std::size_t id) const;

	void merge_graph(const Graph& r, std::vector<std::size_t>& node_mapping);

private:
	std::vector<Principal> nodes_;
	std::vector<std::vector<std::size_t> > out_edges_;
	std::vector<std::vector<std::size_t> > in_edges_;
};

inline Graph operator*(const Principal& l, const Principal& r)
{
	return Graph(l) * Graph(r);
}

inline Graph operator*(const Graph& l, const Principal& r)
{
	return l * Graph(r);
}

inline Graph operator*(const Principal& l, const Graph& r)
{
	return Graph(l) * r;
}

inline Graph operator+(const Principal& l, const Principal& r)
{
	return Graph(l) + Graph(r);
}

inline Graph operator+(const Graph& l, const Principal& r)
{
	return l + Graph(r);
}

inline Graph operator+(const Principal& l, const Graph& r)
{
	return Graph(l) + r;
}

