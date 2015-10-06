// -*- c-basic-offset: 4; related-file-name: "../include/click/xiaifacetable.hh" -*-
#include <click/config.h>
#include <click/xiaifacetable.hh>
#include <click/xiautil.hh>
#include <click/xiapath.hh>


XIAInterface::XIAInterface(String dag, String rv_dag, String rv_control_dag)
{
	_dag = dag;
	_rv_dag = rv_dag;
	_rv_dag_exists = _is_valid_dag(_rv_dag);
	_rv_control_dag = rv_control_dag;
	_rv_control_dag_exists = _is_valid_dag(_rv_control_dag);
}

XIAInterface::~XIAInterface()
{
	_rv_control_dag_exists = false;
	_dag = "";
	_rv_dag = "";
	_rv_control_dag = "";
}

bool XIAInterface::_is_valid_dag(String dag)
{
	return (dag.length() > CLICK_XIA_XID_ID_LEN);
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
	return _rv_dag_exists;
}

bool XIAInterface::has_rv_control_dag()
{
	return _rv_control_dag_exists;
}

bool XIAInterface::update_dag(String dag)
{
	_dag = dag;
	return true;
}

bool XIAInterface::update_rv_dag(String rv_dag)
{
	_rv_dag = rv_dag;
	return true;
}

bool XIAInterface::update_rv_control_dag(String rv_control_dag)
{
	bool retval = false;
	// If the user passed a valid dag, update our dag
	if(rv_control_dag.length() > CLICK_XIA_XID_ID_LEN) {
		_rv_control_dag = rv_control_dag;
		_rv_control_dag_exists = true;
		retval = true;
	} else {
		click_chatter("XIAInterface: Provided rv_control_dag not valid, skip");
		if (_rv_control_dag_exists) {
			click_chatter("XIAInterface: Using: %s", _rv_control_dag.c_str());
		}
	}
	return retval;
}

XIAInterfaceTable::XIAInterfaceTable()
{
	numInterfaces = 0;
	set_default(0);    // For now, first interface is the default
}

XIAInterfaceTable::~XIAInterfaceTable()
{
	interfaceToDag.clear();
	//dagToInterface.clear();
	numInterfaces = 0;
}

// Add entry to both tables
// Return false if entry already exists in either table
bool XIAInterfaceTable::add(int iface, String dag)
{
	// Ensure interfaceToDag does not have an entry for iface
	if(interfaceToDag.find(iface) != interfaceToDag.end()) {
		click_chatter("XIAInterfaceTable::add Entry for %d exists", iface);
		return false;
	}
	// Ensure dagToInterface does not have an entry for dag
	//if(dagToInterface.find(dag) != dagToInterface.end()) {
	//	click_chatter("XIAInterfaceTable::add Entry for %s exists", dag.c_str());
	//	return false;
	//}
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
	// Ensure interfaceToDag contains a matching interface
	if(interfaceToDag.find(iface) == interfaceToDag.end()) {
		click_chatter("XIAInterfaceTable::update Interface %d not found in table", iface);
		return false;
	}
	// Ensure dagToInterface contains a matching DAG
	//if(dagToInterface.find(dag) == dagToInterface.end()) {
	//	click_chatter("XIAInterfaceTable::update DAG %s not found in table", dag.c_str());
	//	return false;
	//}
	// Update both tables
	_insert(iface, dag);
	return true;
}

bool XIAInterfaceTable::update(int iface, XIAPath dag)
{
	return update(iface, dag.unparse());
}

bool XIAInterfaceTable::update_rv_dag(int iface, String dag)
{
	return interfaceToDag[iface].update_rv_dag(dag);
}

bool XIAInterfaceTable::update_rv_control_dag(int iface, String dag)
{
	return interfaceToDag[iface].update_rv_control_dag(dag);
}

// Remove entry from both tables for iface
// Returns false if entry does not exist in either table
bool XIAInterfaceTable::remove(int iface)
{
	// Find the entry for iface in interfaceToDag
	ifaceDagIter = interfaceToDag.find(iface);
	if(ifaceDagIter == interfaceToDag.end()) {
		click_chatter("XIAInterfaceTable::update Removing non-existent interface %d", iface);
		return false;
	}
	// Retrieve DAG and find its entry in dagToInterface
	String dag = ifaceDagIter.value().dag();
	//if(dagToInterface.find(dag) == dagToInterface.end()) {
	//	click_chatter("XIAInterfaceTable::remove Removing non-existent dag %s", dag.c_str());
	//	return false;
	//}
	// Erase entries from both tables
	_erase(iface, dag);
	return true;
}

int XIAInterfaceTable::getIfaceID(String dag)
{
	ifaceDagIter = interfaceToDag.begin();
	for(;ifaceDagIter!=interfaceToDag.end();ifaceDagIter++) {
		if(dag.compare(ifaceDagIter.value().dag()) == 0) {
			return ifaceDagIter.key();
		}
	}
	return -1;
}

// Remove entry from both tables for dag
// Returns false if entry does not exist in either table
/*
bool XIAInterfaceTable::remove(String dag)
{
	// Find the entry for DAG in dagToInterface
	dagIfaceIter = dagToInterface.find(dag);
	if(dagIfaceIter == dagToInterface.end()) {
		click_chatter("XIAInterfaceTable::remove dag not found %s", dag.c_str());
		return false;
	}
	// Retrieve iface and find its entry in interfaceToDag
	int iface = dagIfaceIter.value();
	if(interfaceToDag.find(iface) == interfaceToDag.end()) {
		click_chatter("XIAInterfaceTable::remove iface not found %d", iface);
		return false;
	}
	// Erase entries from both tables
	_erase(iface, dag);
	return true;
}

bool XIAInterfaceTable::remove(XIAPath dag)
{
	return remove(dag.unparse());
}
*/

// Unconditionally insert or update an entry in both tables
void XIAInterfaceTable::_insert(int iface, String dag)
{
	interfaceToDag[iface] = XIAInterface(dag);
	//dagToInterface[dag] = iface;
}

// Erase entry from both tables. Caller makes sure entries exist.
void XIAInterfaceTable::_erase(int iface, String dag)
{
	interfaceToDag.erase(iface);
	//dagToInterface.erase(dag);
	numInterfaces--;
}

