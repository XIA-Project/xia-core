// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaifacetable.cc" -*-
#ifndef XIAIFACETABLE_H
#define XIAIFACETABLE_H

#include <click/xiapath.hh>
#include <click/hashtable.hh>

#define MAX_XIA_INTERFACES 4

class XIAInterfaceTable {
	public:
		XIAInterfaceTable();
		~XIAInterfaceTable();
		bool update(int iface, String dag);
		bool update(int iface, XIAPath dag);
		bool add(int iface, String dag);
		bool add(int iface, XIAPath dag);
		bool remove(int iface);
		bool remove(String dag);
		bool remove(XIAPath dag);
	private:
		bool numentries();
		void _erase(int iface, String dag);
		void _insert(int iface, String dag);

		// Assumption: One DAG per interface. May change in future.
		// Mapping from interface to corresponding DAG
		HashTable<int, String> interfaceToDag;
		HashTable<int, String>::iterator ifaceDagIter;
		// Mapping from DAG to corresponding interface
		HashTable<String, int> dagToInterface;
		HashTable<String, int>::iterator dagIfaceIter;

		int numInterfaces;
};
#endif
