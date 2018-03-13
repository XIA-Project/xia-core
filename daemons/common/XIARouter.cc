#include <string.h>
#include <map>
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
	size_t n;

	if (!connected())
		return XR_NOT_CONNECTED;

	if ((_cserr = _cs.get_config_el_names(elements)) != 0)
		return XR_CLICK_ERROR;

	vector<string>::iterator it;
	for (it = elements.begin(); it < elements.end(); it++) {

		// cheap way of finding host and router devices, they both have a /xrc element
		if ((n = (*it).find("/xrc")) != string::npos) {
			rlist.push_back((*it).substr(0, n));
		}
	}
	return 0;
}

// get the current set of route entries, return value is number of entries returned or < 0 on err
int XIARouter::getRoutes(std::string xidtype, std::map<std::string, XIARouteEntry> &xrt)
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

	std::string table = _router + "/xrc/n/proc/rt_" + xidtype;

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

			XIARouteEntry entry;
			unsigned start, next;
			string s;
			int port;

			start = 0;
			next = line.find(",");
			entry.xid = line.substr(start, next - start);

			start = next + 1;
			next = line.find(",", start);
			s = line.substr(start, next - start);
			port = atoi(s.c_str());
			entry.port = port;

			start = next + 1;
			next = line.find(",", start);
			entry.nextHop = line.substr(start, next - start);

			start = next + 1;
			s = line.substr(start, line.length() - start);
			entry.flags = atoi(s.c_str());

			xrt[entry.xid] = entry;
			n++;
		}
		current++;
	}

	return n;
}

std::string XIARouter::itoa(signed i)
{
	std::string s;
	std::stringstream ss;

	ss << i;
	s = ss.str();
	return s;
}

int XIARouter::rawWrite(const std::string &element, const std::string &cmd, const std::string &data)
{
    if ((_cserr == _cs.write(element, cmd, data)))
        return XR_CLICK_ERROR;

    return XR_OK;
}

int XIARouter::updateRoute(string cmd, const std::string &xid, int port, const std::string &next, unsigned long flags, int weight, const std::string &index)
{
	string xidtype;
	string mutableXID(xid);
	size_t n;

	if (!connected())
		return XR_NOT_CONNECTED;

	if (mutableXID.length() == 0)
		return XR_INVALID_XID;

	if (next.length() > 0 && next.find(":") == string::npos)
		return XR_INVALID_XID;

	n = mutableXID.find(":");
	if (n == string::npos || n >= sizeof(xidtype))
		return XR_INVALID_XID;

	if (getRouter().length() == 0)
		return  XR_ROUTER_NOT_SET;

	if (weight > 100) // weight <= 0 means to delete it
		return XR_INVALID_WEIGHT;

	xidtype = mutableXID.substr(0, n);

	std::string table = _router + "/xrc/n/proc/rt_" + xidtype;

	string default_xid("-");
	if (mutableXID.compare(n+1, 1, default_xid) == 0)
		mutableXID = default_xid;

	std::string entry;

	// remove command only takes an xid
	if (cmd == "remove")
		entry = mutableXID;
	else if (cmd == "sel_remove")
		entry = mutableXID + "," + index;
	else
		entry = mutableXID + "," + itoa(port) + "," + next + "," + itoa(flags) + "," + itoa(weight) + "," + index;

	if ((_cserr = _cs.write(table, cmd, entry)) != 0)
		return XR_CLICK_ERROR;

	return XR_OK;
}

int XIARouter::addRoute(const std::string &xid, int port, const std::string &next, unsigned long flags)
{
	string index = "";
	return updateRoute("add4", xid, port, next, flags, 100, index);
}

int XIARouter::setRoute(const std::string &xid, int port, const std::string &next, unsigned long flags)
{
	string index = "";
	return updateRoute("set4", xid, port, next, flags, 100, index);
}

int XIARouter::appRoute(const std::string &xid, int port, const std::string &next, unsigned long flags, int weight, const std::string &index)
{
	return updateRoute("app4", xid, port, next, flags, weight, index);
}

int XIARouter::seletiveSetRoute(const std::string &xid, int port, const std::string &next, unsigned long flags, int weight, const std::string &index)
{
	return updateRoute("sel_set4", xid, port, next, flags, weight, index);
}

int XIARouter::delRoute(const std::string &xid)
{
	string next = "";
	return updateRoute("remove", xid, 0, next, 0, 100, next);
}

const char *XIARouter::cserror()
{
	switch(_cserr) {
		case ControlSocketClient::no_err:
			return "no error";
		case ControlSocketClient::sys_err:
			return "O/S or networking error, check errno for more information";
		case ControlSocketClient::init_err:
			return "tried to perform operation on an unconfigured ControlSocketClient";
		case ControlSocketClient::reinit_err:
			return "tried to re-configure the client before close()ing it";
		case ControlSocketClient::no_element:
			return "specified element does not exist";
		case ControlSocketClient::no_handler:
			return "specified handler does not exist";
		case ControlSocketClient::handler_no_perm:
			return "router denied access to the specified handler";
		case ControlSocketClient::handler_err:
			return "handler returned an error";
		case ControlSocketClient::handler_bad_format:
			return "bad format in calling handler";
		case ControlSocketClient::click_err:
			return "unexpected response or error from the router";
		case ControlSocketClient::too_short:
			return "user buffer was too short";
	}
	return "unknown";
}
