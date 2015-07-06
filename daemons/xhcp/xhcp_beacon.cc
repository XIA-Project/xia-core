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

/*
XHCPBeacon::XHCPBeacon(int id=-1, std::string name="",
		std::string hid="", std::string ad="", std::string rhid="",
		std::string r4id="", std::string ns_dag="");
		*/

XHCPBeacon::XHCPBeacon(std::string ad, std::string rhid, std::string r4id, std::string ns_dag)
{
	_ndag = ad;
	_router_hid = rhid;
	_router_4id = r4id;
	_nameserver_dag = ns_dag;
}

XHCPBeacon::XHCPBeacon(char *buf)
{

	int i;
	xhcp_pkt *tmp = (xhcp_pkt *)buf;
	xhcp_pkt_entry *entry = (xhcp_pkt_entry *)tmp->data;
	for (i=0; i<tmp->num_entries; i++) {
		switch (entry->type) {
			case XHCP_TYPE_NDAG:
				_ndag = entry->data;
				printf("Router NDAG: %s\n", _ndag.c_str());
				break;
			case XHCP_TYPE_GATEWAY_ROUTER_HID:
				_router_hid = entry->data;
				printf("Router HID: %s\n", _router_hid.c_str());
				break;
			case XHCP_TYPE_GATEWAY_ROUTER_4ID:
				_router_4id = entry->data;
				printf("Router 4ID: %s\n", _router_4id.c_str());
				break;
			case XHCP_TYPE_NAME_SERVER_DAG:
				_nameserver_dag = entry->data;
				printf("Nameserver: %s\n", _nameserver_dag.c_str());
				break;
			default:
				syslog(LOG_WARNING, "invalid xhcp data, discarding...");
				break;
		}
		entry = (xhcp_pkt_entry *)((char *)entry + sizeof(entry->type) + strlen(entry->data) + 1);
	}
}

XHCPBeacon::~XHCPBeacon()
{
	// Cleanup
}

std::string XHCPBeacon::getNetworkDAG()
{
	return _ndag;
}

void XHCPBeacon::setNetworkDAG(std::string ad)
{
	_ndag = ad;
}

std::string XHCPBeacon::getRouterHID()
{
	return _router_hid;
}

void XHCPBeacon::setRouterHID(std::string rhid)
{
	_router_hid = rhid;
}

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
	if(!_ndag.compare(other._ndag)) return false;
	if(!_router_hid.compare(other._router_hid)) return false;
	if(!_router_4id.compare(other._router_4id)) return false;
	if(!_nameserver_dag.compare(other._nameserver_dag)) return false;
	return true;
}

