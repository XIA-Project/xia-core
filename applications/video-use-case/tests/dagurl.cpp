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
#include "dagaddr.hpp"
#include <stdlib.h>
#include <cstdio>

int main()
{
	Node n_src;
	Node n_ad(Node::XID_TYPE_AD, "0606060606060606060606060606060606060606");
	Node n_hid(Node::XID_TYPE_HID, "0101010101010101010101010101010101010101");
	Node n_cid(Node::XID_TYPE_CID, "0202020202020202020202020202020202020202");

	printf("n_ad: %s\n", n_ad.to_string().c_str());
	printf("n_hid: %s\n", n_hid.to_string().c_str());
	printf("n_cid: %s\n\n", n_cid.to_string().c_str());

	// Path directly to n_cid
	// n_src -> n_cid
	Graph g0 = n_src * n_cid;

	// Path to n_cid through n_hid
	// n_src -> n_hid -> n_cid
	Graph g1 = n_src * n_ad * n_cid;

	// Path to n_cid through n_ad then n_hid
	// n_src -> n_ad -> n_hid -> n_cid
	Graph g2 = n_src * n_ad * n_hid * n_cid;

	// Combine the above three paths into a single DAG;
	// g1 and g2 become fallback paths from n_src to n_cid
	Graph g3 = g0 + g1 + g2;

	// Get a DAG string version of the graph that could be used in an
	// XSocket API call
	const char* dag_http_url_string = g3.http_url_string().c_str();
	printf("%s\n", dag_http_url_string);
	printf("%s\n", g3.dag_string().c_str());
	
	printf("g5 = g3 * (SID0 + SID1) * SID2\n");
	Graph g5 = g3 * (Node(Node::XID_TYPE_SID, "0303030303030303030303030303030303030303") + Node(Node::XID_TYPE_SID, "0404040404040404040404040404040404040404")) * Node(Node::XID_TYPE_SID, "0505050505050505050505050505050505050505");
	g5.print_graph();
	printf("\n");
	printf("%s\n\n", g5.dag_string().c_str());
	
	printf("g5_prime = Graph(g5.dag_string())\n");
	Graph g5_prime = Graph(g5.dag_string());
	g5_prime.print_graph();
	printf("\n\n");

	printf("g5_prime2 = Graph(g3)\n");
	Graph g5_prime2 = Graph(g3);
	printf("%s\n\n", g5_prime2.dag_string().c_str());
	printf("g5_prime2 *= SID0\n");
	g5_prime2 *= Node(Node::XID_TYPE_SID, "0303030303030303030303030303030303030303");
	printf("%s\n\n", g5_prime2.dag_string().c_str());
	printf("g5_double = g5 * g3\n");
	Graph g5_double = g5 * g3;
	printf("%s\n\n", g5_double.dag_string().c_str());

	printf("%s\n", g5_double.http_url_string().c_str());
	Graph parsed(g5_double.http_url_string());
	printf("%s\n\n", parsed.dag_string().c_str());

	return 0;
}
