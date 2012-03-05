#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sstream>

using namespace std;
#include "XIARouter.hh"

int XIARouter::connect(std::string clickHost, unsigned short controlPort)
{
	struct hostent *h;

	if (_connected)
		return XR_ALREADY_CONNECTED;

	if ((h = gethostbyname(clickHost.c_str())) == NULL)
		return XR_BAD_HOSTNAME;

	unsigned addr = *(unsigned*)h->h_addr;

	if ((_cserr = _cs.configure(addr, controlPort)) != 0)
		return XR_NOT_CONNECTED;
	
	_connected = true;
	return XR_OK;
}

void XIARouter::close()
{
	if (_connected)
		_cserr = _cs.close();
	_connected = false;
}

int XIARouter::version(std::string &ver)
{
	if (!connected())
		return XR_NOT_CONNECTED;

	if ((_cserr = _cs.get_router_version(ver)) == 0)
		return XR_OK;
	return  XR_CLICK_ERROR;
}

int XIARouter::listRouters(std::vector<std::string> &rlist)
{
	vector<string> elements;
	int n;

	if (!connected())
		return XR_NOT_CONNECTED;

	if ((_cserr = _cs.get_config_el_names(elements)) != 0)
		return XR_CLICK_ERROR;

	vector<string>::iterator it;
	for (it = elements.begin(); it < elements.end(); it++) {

		// cheap way of finding host and router devices, they both have a /cache element
		if ((n = (*it).find("/cache")) != string::npos) {
			rlist.push_back((*it).substr(0, n));
		}
	}
	return 0;
}

// get the current set of route entries, return value is number of entries returned or < 0 on err
int XIARouter::getRoutes(std::string xidtype, std::vector<XIARouteEntry> &xrt)
{
	std::string result;
	vector<string> lines;
	int n = 0;

	if (!connected())
		return XR_NOT_CONNECTED;

	if (xidtype.length() == 0)
		return XR_INVALID_XID;

	if (getRouter().length() == 0)
		return  XR_ROUTER_NOT_SET;

	std::string table = _router + "/n/proc/rt_" + xidtype + "/rt";

	if ((_cserr = _cs.read(table, "list", result)) != 0)
		return XR_CLICK_ERROR;

	unsigned start = 0;
	unsigned current = 0;
	unsigned len = result.length();
	string line;

	xrt.clear();
	while (current < len) {
		start = current;
		while (current < len && result[current] != '\n') {
			current++;
		}

		if (start < current || current < len) {
			line = result.substr(start, current - start);

			if (n > 0) {
				// FIXME: first line of the result is a header, we should just eliminate
				// it in the click side
				XIARouteEntry entry;
				unsigned start, next;
				string s;
				int port;

				start = 0;
				next = line.find(";");
				entry.xid = line.substr(start, next - start);

				start = next + 1;
				s = line.substr(start, line.length() - start);
				port = atoi(s.c_str());
				entry.port = port;

				// FIXME: test code - replace with real stuff
				entry.nextHop = "AD:1111222233334444555566667777888899990000";
				entry.flags = 5;
				xrt.push_back(entry);
			}
			n++;
		}
		current++;
	}

	// return one less because of the header field we skipped
	return n - 1;
}

std::string XIARouter::itoa(unsigned i)
{
	std::string s;
	std::stringstream ss;

	ss << i;
	s = ss.str();
	return s;
}

int XIARouter::updateRoute(string cmd, std::string &xid, unsigned short port, std::string &next, unsigned long /* flags */)
{
	string xidtype;
	unsigned n;

	if (!connected())
		return XR_NOT_CONNECTED;

	if (xid.length() == 0)
		return XR_INVALID_XID;

	if (next.length() > 0 && next.find(":") == string::npos)
		return XR_INVALID_XID;

	n = xid.find(":");
	if (n == string::npos || n >= sizeof(xidtype))
		return XR_INVALID_XID;

	if (getRouter().length() == 0)
		return  XR_ROUTER_NOT_SET;

	xidtype = xid.substr(0, n);

	std::string table = _router + "/n/proc/rt_" + xidtype + "/rt";
	std::string entry = xid + " " + itoa(port);

	// FIXME: will need to add logic here to differentiate remove from set/add once
	// the click changes are made

//	printf("%s ::: %s ::: %s\n", cmd.c_str(), tablec_str(), entry.c_str());

	if ((_cserr = _cs.write(table, cmd, entry)) != 0)
		return XR_CLICK_ERROR;
	
	return XR_OK;
}

int XIARouter::addRoute(std::string &xid, unsigned short port, std::string &next, unsigned long flags)
{
	return updateRoute("add", xid, port, next, flags);
}

int XIARouter::setRoute(std::string &xid, unsigned short port, std::string &next, unsigned long flags)
{
	return updateRoute("set", xid, port, next, flags);
}

int XIARouter::delRoute(std::string &xid)
{
	string next = "";
	return updateRoute("remove", xid, 0, next, 0);
}

