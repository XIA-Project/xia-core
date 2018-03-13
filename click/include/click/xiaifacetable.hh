// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaifacetable.cc" -*-
#ifndef XIAIFACETABLE_H
#define XIAIFACETABLE_H

#include <click/xiapath.hh>
#include <click/hashtable.hh>

#define MAX_XIA_INTERFACES 4

class XIAInterface {

	public:
		XIAInterface(String dag="", String rhid="",
				String rv_dag="", String rv_control_dag="");
		~XIAInterface();

		String dag() {
			return _dag;
		}

		String rv_dag() {
			return _rv_dag;
		}

		String rhid() {
			return _rhid;
		}

		String rv_control_dag() {
			return _rv_control_dag;
		}

		bool has_rhid();
		bool has_rv_dag();
		bool has_rv_control_dag();

		bool update_rhid(String rhid);
		bool update_dag(String dag);
		bool update_rv_dag(String rv_dag);
		bool update_rv_control_dag(String rv_control_dag);
		String hid();

	private:
		bool _is_valid_dag(String dag);

		String _dag;
		String _rhid;
		String _rv_dag;
		bool _rv_dag_exists;
		String _rv_control_dag;
		bool _rv_control_dag_exists;
};

class XIAInterfaceTable {
	public:
		XIAInterfaceTable();
		~XIAInterfaceTable();
		bool update(int iface, String dag);
		bool update(int iface, XIAPath dag);
		bool update_rhid(int iface, String rhid);
		bool update_rv_dag(int iface, String dag);
		bool update_rv_control_dag(int iface, String dag);
		bool add(int iface, String dag);
		bool add(int iface, XIAPath dag);
		bool remove(int iface);
		bool has_rhid(int iface);
		String getRHID(int iface) {
			return interfaces[iface].rhid();
		}
		String getDAG(int iface) {
			return interfaces[iface].dag();
		}
		String getRVDAG(int iface) {
			return interfaces[iface].rv_dag();
		}
		bool hasRVDAG(int iface) {
			return interfaces[iface].has_rv_dag();
		}
		String getRVControlDAG(int iface) {
			return interfaces[iface].rv_control_dag();
		}
		bool hasRVControlDAG(int iface) {
			return interfaces[iface].has_rv_control_dag();
		}
		XIAInterface getInterface(int iface) {
			return interfaces[iface];
		}
		int getIfaceID(String dag);
		int size() {
			return numInterfaces;
		}
		void set_default(int interface) { _default_interface = interface;}
		int default_interface() { return _default_interface;}
		String default_dag() { return interfaces[_default_interface].dag();}
	private:
		void _erase(int iface, String dag);
		void _insert(int iface, String dag);

		// Assumption: One DAG per interface. May change in future.
		// Mapping from interface ID to corresponding interface state
		HashTable<int, XIAInterface> interfaces;
		HashTable<int, XIAInterface>::iterator interfacesIter;

		int numInterfaces;
		int _default_interface;
};
#endif
