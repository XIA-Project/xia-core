#include "dagaddr.hpp"
#include <cstdio>

int main()
{
	Principal p_src;
	Principal p_hid(10, "HID_________________");
	Principal p_cid(11, "CID_________________");

	printf("g0\n");
	Graph g0 = p_src * p_cid;
	g0.print();
	printf("\n");

	printf("g1\n");
	Graph g1 = p_src * p_hid * p_cid;
	g1.print();
	printf("\n");

	printf("g2\n");
	Graph g2 = g0 + g1;
	g2.print();
	printf("\n");

	printf("g3\n");
	Graph g3 = g2 * (Principal(12, "SID0________________") + Principal(12, "SID1________________")) * Principal(12, "SID2________________");
	g3.print();
	printf("\n");

	return 0;
}

