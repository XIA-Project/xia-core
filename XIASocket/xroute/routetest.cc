#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>
#include <vector>
#include "XIARouter.hh"

XIARouter xr;

void listRoutes(std::string xidType)
{
	int rc;

	vector<XIARouteEntry> routes;

	printf("\nroutes for %s\n", xidType.c_str());

	if ((rc = xr.getRoutes(xidType, routes)) > 0) {

		vector<XIARouteEntry>::iterator ir;
		for (ir = routes.begin(); ir < routes.end(); ir++) {
			XIARouteEntry r = *ir;
			printf("%s: %d : %s : %08lx\n", r.xid.c_str(), r.port, r.nextHop.c_str(), r.flags);
		}

	} else if (rc == 0) {
		printf("No routes exist for %s\n", xidType.c_str());
	} else {
		printf("Error getting route list %d\n", rc);
	}
}

int main()
{
	int rc;
	string ver;
	vector<string> routers;

	printf("Click connected: %s\n",  xr.connected() ? "yes" : "no");
	
	if ((rc = xr.connect()) != 0) {
		printf("unable to connect to click! (%d)\n", rc);
		return -1;
	}

	printf("Click connected: %s\n",  xr.connected() ? "yes" : "no");

	if ((rc = xr.version(ver)) == 0)
		printf("Click is version %s\n", ver.c_str());
	else
		printf("error getting click version (%d)\n", rc);


	if ((rc = xr.listRouters(routers)) == 0) {
		printf("ROUTERS\n");
		vector<string>::iterator it;
		for (it = routers.begin(); it < routers.end(); it++)
			printf("%s\n", (*it).c_str());
	} else
		printf("error getting router list (%d)\n", rc);

	xr.setRouter("router0");


	string xid = "AD:1234567890123456789012345678901234567890";
	string nxt = "AD:FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

	listRoutes("AD");

	printf("\nadding route: (should work)\n");
	if ((rc = xr.addRoute(xid, 2, nxt, 0xff)) != 0)
		printf("error adding route %d\n", rc);
//	listRoutes("AD");
	
	printf("adding same route: (should fail)\n");
	if ((rc = xr.addRoute(xid, 2, nxt, 0xff)) != 0)
		printf("error adding route %d\n", rc);
//	listRoutes("AD");
	
	printf("setting same route: (should work)\n");
	if ((rc = xr.setRoute(xid, 3, nxt, 0xffff)) != 0)
		printf("error setting route %d\n", rc);
//	listRoutes("AD");
	
	printf("deleting same route: (should work)\n");
	if ((rc = xr.delRoute(xid)) != 0)
		printf("error deleting route %d\n", rc);
//	listRoutes("AD");
	
	printf("deleting same route: (should fail)\n");
	if ((rc = xr.delRoute(xid)) != 0)
		printf("error deleting route %d\n", rc);
//	listRoutes("AD");
	
	printf("setting same route: (doesn't exist, should work)\n");
	if ((rc = xr.setRoute(xid, 2, nxt, 0xff)) != 0)
		printf("error setting route %d\n", rc);
	listRoutes("AD");


	printf("\nHID fun\n");
	char s[64];
	for (int i = 100; i < 105; i++) {
		sprintf(s, "HID:%040d", i);
		string str = s;
		printf("adding %s\n", s);
		if ((rc = xr.addRoute(str, 2, nxt, 0x12345678)) != 0)
			printf("error setting route %d\n", rc);
	}
	listRoutes("HID");

	xr.close();
	return 0;
}
