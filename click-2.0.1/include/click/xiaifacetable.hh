// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaifacetable.cc" -*-
#ifndef XIAIFACETABLE_H
#define XIAIFACETABLE_H

#include <click/xiapath.hh>
#include <click/hashtable.hh>

#define MAX_XIA_INTERFACES 4

class XIAInterface {

	public:
		XIAInterface(String dag="", String rv_dag="", String rv_control_dag="");
		~XIAInterface();
		String dag() {
			return _dag;
		}
		String rv_control_dag() {
			return _rv_control_dag;
		}
		bool has_rv_control_dag();
		bool has_rv_dag();
		bool update_dag(String dag);
		bool update_rv_dag(String rv_dag);
		bool update_rv_control_dag(String rv_control_dag);
		String hid();

	private:
		bool _is_valid_dag(String dag);

		String _dag;
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
		bool update_rv_dag(int iface, String dag);
		bool update_rv_control_dag(int iface, String dag);
		bool add(int iface, String dag);
		bool add(int iface, XIAPath dag);
		bool remove(int iface);
		//bool remove(String dag);
		//bool remove(XIAPath dag);
		String getDAG(int iface) {
			return interfaceToDag[iface].dag();
		}
		XIAInterface getInterface(int iface) {
			return interfaceToDag[iface];
		}
		int getIfaceID(String dag);
		int size() {
			return numInterfaces;
		}
		void set_default(int interface) { _default_interface = interface;}
		int default_interface() { return _default_interface;}
		String default_dag() { return interfaceToDag[_default_interface].dag();}
	private:
		void _erase(int iface, String dag);
		void _insert(int iface, String dag);

		// Assumption: One DAG per interface. May change in future.
		// Mapping from interface to corresponding DAG
		HashTable<int, XIAInterface> interfaceToDag;
		HashTable<int, XIAInterface>::iterator ifaceDagIter;
		// Mapping from DAG to corresponding interface
		//HashTable<String, int> dagToInterface;
		//HashTable<String, int>::iterator dagIfaceIter;

		int numInterfaces;
		int _default_interface;
};
#endif
