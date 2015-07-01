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
#include "xhcp_interface.hh"
#include "../common/XIARouter.hh"
#include "dagaddr.hpp"

/*
XHCPInterface::XHCPInterface(int id=-1, std::string name="",
		std::string hid="", std::string ad="", std::string rhid="",
		std::string r4id="", std::string ns_dag="");
		*/

XHCPInterface::XHCPInterface(int id, bool active, std::string name, std::string hid, std::string ad, std::string rhid, std::string r4id, std::string ns_dag)
{
	_id = id;
	_active = active;
	_name = name;
	_hid = hid;
	_ad = ad;
	_router_hid = rhid;
	_router_4id = r4id;
	_nameserver_dag = ns_dag;
}

XHCPInterface::~XHCPInterface()
{
	// Cleanup
}

int XHCPInterface::getID()
{
	return _id;
}

void XHCPInterface::setID(int id)
{
	_id = id;
}

std::string XHCPInterface::getName()
{
	return _name;
}

void XHCPInterface::setName(std::string name)
{
	_name = name;
}

std::string XHCPInterface::getHID()
{
	return _hid;
}

void XHCPInterface::setHID(std::string hid)
{
	_hid = hid;
}

std::string XHCPInterface::getAD()
{
	return _ad;
}

void XHCPInterface::setAD(std::string ad)
{
	_ad = ad;
}

std::string XHCPInterface::getRouterHID()
{
	return _router_hid;
}

void XHCPInterface::setRouterHID(std::string rhid)
{
	_router_hid = rhid;
}

std::string XHCPInterface::getRouter4ID()
{
	return _router_4id;
}

void XHCPInterface::setRouter4ID(std::string r4id)
{
	_router_4id = r4id;
}

std::string XHCPInterface::getNameServerDAG()
{
	return _nameserver_dag;
}

void XHCPInterface::setNameServerDAG(std::string ns_dag)
{
	_nameserver_dag = ns_dag;
}

bool XHCPInterface::isActive()
{
	return _active;
}

bool XHCPInterface::operator==(const XHCPInterface& other)
{
	if(_id != other._id) return false;
	if(!_ad.compare(other._ad)) return false;
	if(!_router_hid.compare(other._router_hid)) return false;
	if(!_router_4id.compare(other._router_4id)) return false;
	if(!_nameserver_dag.compare(other._nameserver_dag)) return false;
	return true;
}

