/*
** Copyright 2011 Carnegie Mellon University
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
#include "Xsocket.h"
#include "xns.h"
#include "xhcp.hh"
#include "xhcp_beacon.hh"
#include "dagaddr.hpp"
#include<syslog.h>
#include <sstream>

XHCPBeacon::XHCPBeacon(std::string dag, std::string rhid, std::string r4id, std::string ns_dag)
{
	_dag = dag;
	_router_hid = rhid;
	_router_4id = r4id;
	_nameserver_dag = ns_dag;
}

XHCPBeacon::XHCPBeacon(char *buf)
{
	std::string buffer(buf);
	std::istringstream ss(buffer);
	std::getline(ss, _dag, ',');
	std::getline(ss, _router_4id, ',');
	std::getline(ss, _nameserver_dag, ',');
}

XHCPBeacon::~XHCPBeacon()
{
	// Cleanup
}

std::string XHCPBeacon::getRouterDAG()
{
	return _dag;
}

void XHCPBeacon::setRouterDAG(std::string dag)
{
	_dag = dag;
	Graph g(dag.c_str());
	_router_hid = g.intent_HID_str();
}

std::string XHCPBeacon::getRouterHID()
{
	return _router_hid;
}
// Note: no setRouterHID() because _router_hid is set by setRouterDAG

std::string XHCPBeacon::getRouter4ID()
{
	return _router_4id;
}

void XHCPBeacon::setRouter4ID(std::string r4id)
{
	_router_4id = r4id;
}

std::string XHCPBeacon::getNameServerDAG()
{
	return _nameserver_dag;
}

void XHCPBeacon::setNameServerDAG(std::string ns_dag)
{
	_nameserver_dag = ns_dag;
}

bool XHCPBeacon::operator==(const XHCPBeacon& other)
{
	if(!_dag.compare(other._dag)) return false;
	if(!_router_hid.compare(other._router_hid)) return false;
	if(!_router_4id.compare(other._router_4id)) return false;
	if(!_nameserver_dag.compare(other._nameserver_dag)) return false;
	return true;
}

