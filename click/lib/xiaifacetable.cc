// -*- c-basic-offset: 4; related-file-name: "../include/click/xiaifacetable.hh" -*-
#include <click/config.h>
#include <click/xiaifacetable.hh>
#include <click/xiautil.hh>
#include <click/xiapath.hh>


XIAInterface::XIAInterface(String dag, String rhid,
		String rv_dag, String rv_control_dag)
{
	_dag = dag;
	_rhid = rhid;
	_rv_dag = rv_dag;
	_rv_control_dag = rv_control_dag;
}

XIAInterface::~XIAInterface()
{
	_dag = "";
	_rhid = "";
	_rv_dag = "";
	_rv_control_dag = "";
}

bool XIAInterface::_is_valid_dag(String dag)
{
	return (dag.length() > CLICK_XIA_XID_ID_LEN);
}

bool XIAInterface::has_rhid()
{
	return (_rhid.length() >= CLICK_XIA_XID_ID_LEN);
}

String XIAInterface::hid()
{
	XIAPath dag;
	if(!dag.parse(_dag)) {
		return "";
	}
	return dag.xid(dag.destination_node()).unparse();
}

bool XIAInterface::has_rv_dag()
{
	return _is_valid_dag(_rv_dag);
}

bool XIAInterface::has_rv_control_dag()
{
	return _is_valid_dag(_rv_control_dag);
}

bool XIAInterface::update_dag(String dag)
{
	_dag = dag;
	return true;
}

bool XIAInterface::update_rhid(String rhid)
{
	_rhid = rhid;
	return true;
}

bool XIAInterface::update_rv_dag(String rv_dag)
{
	// We never overwrite a known RV DAG. Start instance in home network.
	if(_rv_dag.length() > CLICK_XIA_XID_ID_LEN) {
		click_chatter("XIAInterface: RV DAG:%s", _rv_dag.c_str());
		click_chatter("XIAInterface: Ignored:%s", rv_dag.c_str());
		return true;
	}

	// If the user passed a valid DAG, update our DAG
	if((rv_dag.length() < CLICK_XIA_XID_ID_LEN)) {
		click_chatter("XIAInterface: ERROR: Invalid RV DAG");
		return false;
	}

	_rv_dag = rv_dag;
	click_chatter("XIAInterface: new RVC DAG: %s", _rv_control_dag.c_str());

	return true;
}

bool XIAInterface::update_rv_control_dag(String rv_control_dag)
{
	// We never overwrite a known RV Control DAG assigned by home network.
	if(_rv_control_dag.length() > CLICK_XIA_XID_ID_LEN) {
		click_chatter("XIAInterface: RVC dag:%s", _rv_control_dag.c_str());
		click_chatter("XIAInterface: Ignored:%s", rv_control_dag.c_str());
		return true;
	}

	// If the user passed a valid DAG, update our DAG
	if(rv_control_dag.length() < CLICK_XIA_XID_ID_LEN) {
		click_chatter("XIAInterface: ERROR: Invalid RV Control DAG");
		return false;
	}

	_rv_control_dag = rv_control_dag;
	click_chatter("XIAInterface: new RVC DAG: %s", _rv_control_dag.c_str());

	return true;
}

XIAInterfaceTable::XIAInterfaceTable()
{
	numInterfaces = 0;
	set_default(0);    // For now, first interface is the default
}

XIAInterfaceTable::~XIAInterfaceTable()
{
	interfaces.clear();
	//dagToInterface.clear();
	numInterfaces = 0;
}

// Add entry to both tables
// Return false if entry already exists in either table
bool XIAInterfaceTable::add(int iface, String dag)
{
	// Ensure interfaces does not have an entry for iface
	/* TODO: Nitin: This find() causes a segfault. Fix it.
	if(interfaces.find(iface) != interfaces.end()) {
		click_chatter("XIAInterfaceTable::add Entry for %d exists", iface);
		return false;
	}
	*/

	// Ensure we haven't exceeded the number of allowed interfaces
	if(numInterfaces >= MAX_XIA_INTERFACES) {
		click_chatter("XIAInterfaceTable::add MAX_INTERFACES already allocated");
		return false;
	}
	_insert(iface, dag);
	numInterfaces++;
	return true;
}

bool XIAInterfaceTable::add(int iface, XIAPath dag)
{
	return add(iface, dag.unparse());
}

// Update existing entry in both tables.
// Return false if entry does not exist in either table
bool XIAInterfaceTable::update(int iface, String dag)
{
	// Ensure interfaces contains a matching interface
	if(interfaces.find(iface) == interfaces.end()) {
		click_chatter("XIAInterfaceTable::update Interface %d not found in table", iface);
		return false;
	}
	_insert(iface, dag);
	return true;
}

bool XIAInterfaceTable::update(int iface, XIAPath dag)
{
	return update(iface, dag.unparse());
}

bool XIAInterfaceTable::update_rhid(int iface, String rhid)
{
	return interfaces[iface].update_rhid(rhid);
}

bool XIAInterfaceTable::update_rv_dag(int iface, String dag)
{
	return interfaces[iface].update_rv_dag(dag);
}

bool XIAInterfaceTable::update_rv_control_dag(int iface, String dag)
{
	return interfaces[iface].update_rv_control_dag(dag);
}

// Remove entry from XIAInterfaceTable
// Returns false if entry does not exist
bool XIAInterfaceTable::remove(int iface)
{
	// Find the entry for iface in interfaces
	interfacesIter = interfaces.find(iface);
	if(interfacesIter == interfaces.end()) {
		click_chatter("XIAInterfaceTable::update Removing non-existent interface %d", iface);
		return false;
	}
	// Retrieve DAG and find its entry in dagToInterface
	String dag = interfacesIter.value().dag();
	_erase(iface, dag);
	return true;
}

int XIAInterfaceTable::getIfaceID(String dag)
{
	interfacesIter = interfaces.begin();
	for(;interfacesIter!=interfaces.end();interfacesIter++) {
		if(dag.compare(interfacesIter.value().dag()) == 0) {
			return interfacesIter.key();
		}
	}
	return -1;
}

bool XIAInterfaceTable::has_rhid(int iface)
{
	return interfaces[iface].has_rhid();
}

// Unconditionally insert or update an entry in XIAInterfaceTable
void XIAInterfaceTable::_insert(int iface, String dag)
{
	interfaces[iface] = XIAInterface(dag);
}

// Erase entry. Caller makes sure entry exist.
void XIAInterfaceTable::_erase(int iface, String /* dag */)
{
	interfaces.erase(iface);
	numInterfaces--;
}
