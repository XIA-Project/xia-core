// -*- c-basic-offset: 4; related-file-name: "../../lib/xidtuple.cc" -*-
#ifndef CLICK_XIDTUPLE_HH
#define CLICK_XIDTUPLE_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/xid.hh>

CLICK_DECLS
class StringAccum;
class Element;

class XIDtuple{
public:
	XIDtuple();

	XIDtuple(const XID& a, const XID& b, const XID& c);

	void set_a(const XID& a);
	void set_b(const XID& b);
	void set_c(const XID& c);
	XID& a();
	XID& b();
	XID& c();

	inline uint32_t hashcode() const;

	inline bool operator==(const XIDtuple&) const;
	inline bool operator!=(const XIDtuple&) const;

	void dump();

 private:
	XID _a;
	XID _b;
	XID _c;
} ;


inline uint32_t XIDtuple::hashcode() const
{
    uint32_t a = reinterpret_cast<const uint32_t*>(&_a)[4]; // pick some word in the middle
	uint32_t b = reinterpret_cast<const uint32_t*>(&_b)[4]; // pick some word in the middle
    uint32_t c = reinterpret_cast<const uint32_t*>(&_c)[4]; // pick some word in the middle
    return a + b + c;
}

/** @brief Return if the addresses are the same. */
inline bool
XIDtuple::operator==(const XIDtuple& rhs) const
{
    return reinterpret_cast<const uint32_t*>(&_a)[0] == reinterpret_cast<const uint32_t*>(&rhs._a)[0] &&
           reinterpret_cast<const uint32_t*>(&_a)[1] == reinterpret_cast<const uint32_t*>(&rhs._a)[1] &&
           reinterpret_cast<const uint32_t*>(&_a)[2] == reinterpret_cast<const uint32_t*>(&rhs._a)[2] &&
           reinterpret_cast<const uint32_t*>(&_a)[3] == reinterpret_cast<const uint32_t*>(&rhs._a)[3] &&
           reinterpret_cast<const uint32_t*>(&_a)[4] == reinterpret_cast<const uint32_t*>(&rhs._a)[4] &&
           reinterpret_cast<const uint32_t*>(&_a)[5] == reinterpret_cast<const uint32_t*>(&rhs._a)[5] &&
           reinterpret_cast<const uint32_t*>(&_b)[0] == reinterpret_cast<const uint32_t*>(&rhs._b)[0] &&
           reinterpret_cast<const uint32_t*>(&_b)[1] == reinterpret_cast<const uint32_t*>(&rhs._b)[1] &&
           reinterpret_cast<const uint32_t*>(&_b)[2] == reinterpret_cast<const uint32_t*>(&rhs._b)[2] &&
           reinterpret_cast<const uint32_t*>(&_b)[3] == reinterpret_cast<const uint32_t*>(&rhs._b)[3] &&
           reinterpret_cast<const uint32_t*>(&_b)[4] == reinterpret_cast<const uint32_t*>(&rhs._b)[4] &&
           reinterpret_cast<const uint32_t*>(&_b)[5] == reinterpret_cast<const uint32_t*>(&rhs._b)[5] &&
		   reinterpret_cast<const uint32_t*>(&_c)[0] == reinterpret_cast<const uint32_t*>(&rhs._c)[0] &&
		   reinterpret_cast<const uint32_t*>(&_c)[1] == reinterpret_cast<const uint32_t*>(&rhs._c)[1] &&
		   reinterpret_cast<const uint32_t*>(&_c)[2] == reinterpret_cast<const uint32_t*>(&rhs._c)[2] &&
		   reinterpret_cast<const uint32_t*>(&_c)[3] == reinterpret_cast<const uint32_t*>(&rhs._c)[3] &&
		   reinterpret_cast<const uint32_t*>(&_c)[4] == reinterpret_cast<const uint32_t*>(&rhs._c)[4] &&
		   reinterpret_cast<const uint32_t*>(&_c)[5] == reinterpret_cast<const uint32_t*>(&rhs._c)[5];
}

/** @brief Return if the addresses are different. */
inline bool
XIDtuple::operator!=(const XIDtuple& rhs) const
{
    return reinterpret_cast<const uint32_t*>(&_a)[0] != reinterpret_cast<const uint32_t*>(&rhs._a)[0] ||
           reinterpret_cast<const uint32_t*>(&_a)[1] != reinterpret_cast<const uint32_t*>(&rhs._a)[1] ||
           reinterpret_cast<const uint32_t*>(&_a)[2] != reinterpret_cast<const uint32_t*>(&rhs._a)[2] ||
           reinterpret_cast<const uint32_t*>(&_a)[3] != reinterpret_cast<const uint32_t*>(&rhs._a)[3] ||
           reinterpret_cast<const uint32_t*>(&_a)[4] != reinterpret_cast<const uint32_t*>(&rhs._a)[4] ||
           reinterpret_cast<const uint32_t*>(&_a)[5] != reinterpret_cast<const uint32_t*>(&rhs._a)[5] ||
           reinterpret_cast<const uint32_t*>(&_b)[0] != reinterpret_cast<const uint32_t*>(&rhs._b)[0] ||
           reinterpret_cast<const uint32_t*>(&_b)[1] != reinterpret_cast<const uint32_t*>(&rhs._b)[1] ||
           reinterpret_cast<const uint32_t*>(&_b)[2] != reinterpret_cast<const uint32_t*>(&rhs._b)[2] ||
           reinterpret_cast<const uint32_t*>(&_b)[3] != reinterpret_cast<const uint32_t*>(&rhs._b)[3] ||
           reinterpret_cast<const uint32_t*>(&_b)[4] != reinterpret_cast<const uint32_t*>(&rhs._b)[4] ||
           reinterpret_cast<const uint32_t*>(&_b)[5] != reinterpret_cast<const uint32_t*>(&rhs._b)[5] ||
		   reinterpret_cast<const uint32_t*>(&_c)[0] != reinterpret_cast<const uint32_t*>(&rhs._c)[0] ||
		   reinterpret_cast<const uint32_t*>(&_c)[1] != reinterpret_cast<const uint32_t*>(&rhs._c)[1] ||
		   reinterpret_cast<const uint32_t*>(&_c)[2] != reinterpret_cast<const uint32_t*>(&rhs._c)[2] ||
		   reinterpret_cast<const uint32_t*>(&_c)[3] != reinterpret_cast<const uint32_t*>(&rhs._c)[3] ||
		   reinterpret_cast<const uint32_t*>(&_c)[4] != reinterpret_cast<const uint32_t*>(&rhs._c)[4] ||
		   reinterpret_cast<const uint32_t*>(&_c)[5] != reinterpret_cast<const uint32_t*>(&rhs._c)[5];
}

CLICK_ENDDECLS
#endif
