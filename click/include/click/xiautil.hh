// -*- c-basic-offset: 4; related-file-name: "../../lib/xiautil.cc" -*-
#ifndef CLICK_XIAUTIL_H
#define CLICK_XIAUTIL_H
#include <click/string.hh>
#include <click/hashtable.hh>
#include <click/glue.hh>

CLICK_DECLS

#define SOURCE_DIR "xia-core"

char *xiaRootDir(char *buf, unsigned len);

class XidMap {
public:
	typedef HashTable<int, String> XMap;

	static int id(const String &name) {
		int id = -1;
		 for (XMap::iterator it = xmap.begin(); it; ++it) {
		 	if (name.compare(it.value()) == 0)
		 		id = it.key(); 
		 }
		 return id;
	};
	static bool name(int id, String& name) {
		name =  xmap.get(id);
		bool empty = !name;
		return !empty;
	};
private:
	static XMap load();

	static XMap xmap;

};

CLICK_ENDDECLS

#endif
