#include "Xsocket.h"
#include "dagaddr.hpp"
#include "Xkeys.h"

const char *name = "test.cmu.xia";

int main()
{
	char sid[64];

	struct addrinfo hints;
	struct addrinfo *pai;
	sockaddr_x *psa;
	sockaddr_x sax;
	char s[XIA_MAX_DAG_STR_SIZE];

	// RE dag string with a primay path direct to the SID and a fallback that goes through the AD & HID
	// NOTE: spaces are important in RE formatted DAGS!
	const char *re = "RE ( AD:24b1979de47c480b786b45f1c57b4d19d2681a25 HID:d5b3cf4904b8f8462fbb916ae54ea55ad7fd8836 ) SID:69276369b5dc391ab12895b928cac945fd82a5d2";

	// convert any RE, DAG, or URL formatted sting into a sockaddr_x
	xia_pton(AF_XIA, re, &sax);

	// graph is the native class that does DAG management
	// and is used internally in all XIA functions that manipulate DAGs
	// it is needed if the URL form of the DAG is required
	Graph g(&sax);
	printf("The same DAG in all 3 string representations\n");
	printf("RE: format:(input only)\n%s\n", re);
	printf("DAG format:\n%s\n", g.dag_string().c_str());
	printf("URL format:\n%s\n\n", g.http_url_string().c_str());

	// register the name/dag with the nameserver
	XregisterName(name, &sax);

	// use Xgetaddrinfo to create a DAG using the SID and AD and HID of the host with a fallback path
	// hints is not needed if a DAG with no fallbacks is sufficient
	// create a unique SID
	XmakeNewSID(sid, sizeof(sid));

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_XIA;
	hints.ai_flags = XAI_FALLBACK;
	Xgetaddrinfo(NULL, sid, &hints, &pai);
	psa = (sockaddr_x*)pai->ai_addr;

	// print the address in DAG format
	xia_ntop(AF_XIA, psa, s, sizeof(s));
	printf("New DAG constructed by Xgetaddrinfo:\n%s\n\n", s);
	Xfreeaddrinfo(pai);

	// Create a DAG by hand
	Node src;
	Node n_ad("AD:1234567890123456789012345678901234567890");
	Node n_hid("HID:1111111111111111111111111111111111111111");
	Node n_sid("SID:2222222222222222222222222222222222222222");

	Graph straight = src * n_ad * n_hid * n_sid;

	printf ("straight DAG created by hand\n%s\n\n", straight.dag_string().c_str());

	// add a fallback path
	Graph sidonly = src * n_sid;
	Graph fallback = sidonly + straight;

	printf ("fallback DAG created by hand\n%s\n\n", fallback.dag_string().c_str());


	// look up the entry we created earlier in the name server
	Xgetaddrinfo(name, NULL, NULL, &pai);
	psa = (sockaddr_x*)pai->ai_addr;

	xia_ntop(AF_XIA, psa, s, sizeof(s));
	printf("DAG retrieved from the name server:\n%s\n", s);

	Xfreeaddrinfo(pai);
}

