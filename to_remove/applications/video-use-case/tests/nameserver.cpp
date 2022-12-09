#include <stdlib.h>
#include <cstdio>
#include "../src/utils.h"
#include "Xsocket.h"
#include "dagaddr.hpp"
#include "dagaddr.h"
#include "Xkeys.h"

#define CDN1_NAME "www.cdn1.xia"
#define CDN2_NAME "www.cdn2.xia"
#define CDN3_NAME "www.cdn3.xia"

#define MULTI_CDN1_NAME "www.video1.xia"
#define MULTI_CDN2_NAME "www.video2.xia"
#define MULTI_CDN3_NAME "www.video3.xia"

void testSingleCDNNameMapping(std::string CDN_Name, sockaddr_x addr1, sockaddr_x addr2, sockaddr_x addr3,
								sockaddr_x addr4){
	sockaddr_x addrs[4];
	addrs[0] = addr1;
	addrs[1] = addr2;
	addrs[2] = addr3;
	addrs[3] = addr4;

	for (int i = 0; i < 4; ++i){
		// register server DAG to CDN name
    	if(XregisterAnycastName(CDN_Name.c_str(), &addrs[i]) < 0){
        	die(-1, "cannot register anycast name\n");
    	}
	}

	for(int i = 207; i >= 0; i--){
		// register server DAG to CDN name
    	if(XregisterAnycastName(CDN_Name.c_str(), &addrs[i%4]) < 0){
        	die(-1, "cannot register anycast name\n");
    	}
	}

    for(int i = 0; i < 107; i++){
    	Graph got_g, expected_g;
    	sockaddr_x got_dag, expected_dag;
    	socklen_t daglen = sizeof(got_dag);

    	expected_dag = addrs[i % 4];
    	expected_g.from_sockaddr(&expected_dag);

    	// get the DNS DAG associated with the CDN service name
    	if (XgetDAGbyAnycastName(CDN_Name.c_str(), &got_dag, &daglen) < 0){
        	die(-1, "unable to locate CDN DNS service name: %s\n", CDN_Name.c_str());
    	}
   	 	got_g.from_sockaddr(&got_dag);

    	if(expected_g.dag_string() != got_g.dag_string()){
    		say("look up %s got %s,  expected is %s\n", CDN_Name.c_str(), got_g.dag_string().c_str(), expected_g.dag_string().c_str());
    		die(-1, "expected doesn't match got_g!\n");
    	}
    }
}

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

	sockaddr_x addr0;
	g0.fill_sockaddr(&addr0);

	sockaddr_x addr1;
	g1.fill_sockaddr(&addr1);

	sockaddr_x addr2;
	g2.fill_sockaddr(&addr2);

	sockaddr_x addr3;
	g3.fill_sockaddr(&addr3);

	std::string CDN_NAMES[3];
	CDN_NAMES[0] = CDN1_NAME;
	CDN_NAMES[1] = CDN2_NAME;
	CDN_NAMES[2] = CDN3_NAME;

	for(int i = 0; i < 3; i++){
		testSingleCDNNameMapping(CDN_NAMES[i], addr0, addr1, addr2, addr3);
	}

	return 0;
}
