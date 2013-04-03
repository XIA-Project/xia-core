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

	// TODO: cut here in the example version; stuff below is for testing

	printf("\n\n");
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





	Node n_ad2(Node::XID_TYPE_AD, "0707070707070707070707070707070707070707");
	Node n_hid2(Node::XID_TYPE_HID, "0808080808080808080808080808080808080808");
	Node n_sid("SID:0909090909090909090909090909090909090909");

	printf("g6 = g3 * ((n_cid * n_sid) + (n_cid * n_ad2 * n_sid) + (n_cid * n_ad2 * n_hid2 * n_sid))\n");
	Graph g6 = g3 * ((n_cid * n_sid) + (n_cid * n_ad2 * n_sid) + (n_cid * n_ad2 * n_hid2 * n_sid));
	printf("%s\n\n", g6.dag_string().c_str());

	printf("Testing is_final_intent vvv\n");

	printf("g3.is_final_intent(n_cid): %s\n", (g3.is_final_intent(n_cid))?"true":"false");
	printf("g3.is_final_intent(n_hid): %s\n", (g3.is_final_intent(n_hid))?"true":"false");
	printf("g3.is_final_intent(n_ad): %s\n", (g3.is_final_intent(n_ad))?"true":"false");
	
	printf("g3.is_final_intent(n_cid.id_string()): %s\n", (g3.is_final_intent(n_cid.id_string()))?"true":"false");
	printf("g3.is_final_intent(n_hid.id_string()): %s\n", (g3.is_final_intent(n_hid.id_string()))?"true":"false");
	printf("g3.is_final_intent(n_ad.id_string()): %s\n", (g3.is_final_intent(n_ad.id_string()))?"true":"false");

	printf("Testing is_final_intent ^^^\n\n\n");

	printf("Testing next_hop vvv\n");

	printf("g5.next_hop(n_src):\n%s\n", g5.next_hop(n_src).dag_string().c_str());
	printf("g5.next_hop(n_cid):\n%s\n", g5.next_hop(n_cid).dag_string().c_str());
	printf("g6.next_hop(n_src):\n%s\n", g6.next_hop(n_src).dag_string().c_str());
	printf("g6.next_hop(n_cid):\n%s\n\n", g6.next_hop(n_cid).dag_string().c_str());

	printf("g5.next_hop(n_src.id_string()):\n%s\n", g5.next_hop(n_src.id_string()).dag_string().c_str());
	printf("g5.next_hop(n_cid.id_string()):\n%s\n", g5.next_hop(n_cid.id_string()).dag_string().c_str());
	printf("g6.next_hop(n_src.id_string()):\n%s\n", g6.next_hop(n_src.id_string()).dag_string().c_str());
	printf("g6.next_hop(n_cid.id_string()):\n%s\n", g6.next_hop(n_cid.id_string()).dag_string().c_str());

	printf("Testing next_hop ^^^\n\n\n");


	printf("Testing first_hop vvv\n");

	printf("g5.first_hop():\n%s\n", g5.first_hop().dag_string().c_str());
	printf("g6.first_hop():\n%s\n", g6.first_hop().dag_string().c_str());

	printf("Testing first_hop ^^^\n\n\n");
	
	
	printf("Testing next_hop ^^^\n\n\n");


	printf("Testing construct_from_re_string vvv\n");

	Graph g7 = Graph("RE AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000 SID:1110000000000000000000000000000000001113");
	printf("Graph(\"RE AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000 SID:1110000000000000000000000000000000001113\")\n%s\n", g7.dag_string().c_str());
	Graph g8 = Graph("RE ( AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000 ) SID:1110000000000000000000000000000000001113");
	printf("Graph(\"RE ( AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000 ) SID:1110000000000000000000000000000000001113\")\n%s\n", g8.dag_string().c_str());
	//Graph g9 = Graph("RE ( IP:4500000000010000fafa00000000000000000000 ) AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000 SID:0f00000000000000000000000000000000008888");
	//printf("Graph(\"RE ( IP:4500000000010000fafa00000000000000000000 ) AD:1000000000000000000000000000000000000000 HID:0000000000000000000000000000000000000000 SID:0f00000000000000000000000000000000008888\")\n%s\n", g9.dag_string().c_str());

	printf("Testing construct_from_re_string ^^^\n\n\n");
	

	printf("Testing sockaddr_x vvv\n");

	sockaddr_x *s = (sockaddr_x*)malloc(sizeof(sockaddr_x));
	g6.fill_sockaddr(s);
	Graph g6_prime = Graph(s);
	printf("g6_prime.dag_string().c_str():\n%s\n", g6_prime.dag_string().c_str());

	g7.fill_sockaddr(s);
	Graph g7_prime = Graph(s);
	printf("g7_prime.dag_string().c_str():\n%s\n", g7_prime.dag_string().c_str());

	printf("Testing sockaddr_x ^^^\n\n\n");

	printf("Testing replace_final_intent vvv\n");
	Graph g6_new_intent = Graph(g6);
	g6_new_intent.replace_final_intent(n_cid);
	printf("g6_new_intent.dag_string().c_str():\n%s\n", g6_new_intent.dag_string().c_str());
	printf("Testing replace_final_intent ^^^\n\n\n");

	return 0;
}
