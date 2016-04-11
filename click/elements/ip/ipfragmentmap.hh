#ifndef CLICK_IPFRAGMENTMAP_HH
#define CLICK_IPFRAGMENTMAP_HH 1
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/ipflowid.hh>
CLICK_DECLS

class IPFragmentMap { public:

    IPFragmentMap(unsigned capacity)
	: _capacity(capacity) {
    }

    Packet *handle(Packet *p);

  private:

    struct Entry {
	IPFlowID key;		// sport is ip_id, dport is ip_p
	uint16_t sport;
	uint16_t dport;
	click_jiffies_t arrival_j;
	int place;
	Packet *queue;
	Entry *_hashnext;

	Entry(const IPFlowID &key_)
	    : key(key_) {
	}

	typedef IPFlowID key_type;
	typedef const IPFlowID &key_const_reference;
	key_const_reference hashkey() const {
	    return key;
	}
    };

    struct less {
	less() {
	}
	bool operator()(Entry *a, Entry *b) {
	    return click_jiffies_less(a->arrival_j, b->arrival_j);
	}
    };

    struct place {
	place(Entry **begin)
	    : _begin(begin) {
	}
	void operator()(Entry **it) {
	    (*it)->place = it - _begin;
	}
      private:
	Entry **_begin;
    };

    HashContainer<Entry> _entries;
    SizedHashAllocator<Entry> _allocator;
    Vector<Entry *> _heap;
    unsigned _capacity;

};

CLICK_ENDDECLS
#endif
