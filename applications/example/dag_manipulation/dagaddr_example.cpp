/*
** Copyright 2013 Carnegie Mellon University
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
#include <dagaddr.hpp>
#include <cstdio>

int main()
{
	Node n_src;
	Node n_ad(XID_TYPE_AD, "0606060606060606060606060606060606060606");
	Node n_hid(XID_TYPE_HID, "0101010101010101010101010101010101010101");
	Node n_cid(XID_TYPE_CID, "0202020202020202020202020202020202020202");

	// Path directly to n_cid
	// n_src -> n_cid
	printf("g0 = n_src * n_cid\n");
	Graph g0 = n_src * n_cid;
	g0.print_graph();
	printf("\n");

	// Path to n_cid through n_hid
	// n_src -> n_hid -> n_cid
	printf("g1 = n_src * n_ad * n_cid\n");
	Graph g1 = n_src * n_ad * n_cid;
	g1.print_graph();
	printf("\n");

	// Path to n_cid through n_ad then n_hid
	// n_src -> n_ad -> n_hid -> n_cid
	printf("g2 = n_src * n_ad * n_hid * n_cid\n");
	Graph g2 = n_src * n_ad * n_hid * n_cid;
	g2.print_graph();
	printf("\n");

	// Combine the above three paths into a single DAG;
	// g1 and g2 become fallback paths from n_src to n_cid
	printf("g3 = g0 + g1 + g2\n");
	Graph g3 = g0 + g1 + g2;
	g3.print_graph();
	printf("\n");

	// Get a DAG string version of the graph that could be used in an
	// XSocket API call
	const char* dag_string = g3.dag_string().c_str();
	printf("%s\n", dag_string);

	// Create a DAG from a string (which we might have gotten from an Xsocket
	// API call like XrecvFrom)
	Graph g4 = Graph(dag_string);
	g4.print_graph();
	printf("\n");

	return 0;
}

