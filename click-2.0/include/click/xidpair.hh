// -*- c-basic-offset: 4; related-file-name: "../../lib/xidpair.cc" -*-
#ifndef CLICK_XIDPAIR_HH
#define CLICK_XIDPAIR_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/xid.hh> 

CLICK_DECLS
class StringAccum;
class Element;

class XIDpair{ public:
	XIDpair();
	
	XIDpair(const XID& src, const XID& dst);
	
	void set_src(const XID& src);
	void set_dst(const XID& dst);
	XID& src();
	XID& dst();
	
	inline uint32_t hashcode() const;
	
	inline bool operator==(const XIDpair&) const;
        inline bool operator!=(const XIDpair&) const;
	
     private:	
    	XID src_xid;
    	XID dst_xid;
} ;    


inline uint32_t XIDpair::hashcode() const
{
    uint32_t a = reinterpret_cast<const uint32_t*>(&src_xid)[4]; // pick some word in the middle
    uint32_t b = reinterpret_cast<const uint32_t*>(&dst_xid)[4]; // pick some word in the middle
    return a+b;
}    

/** @brief Return if the addresses are the same. */
inline bool
XIDpair::operator==(const XIDpair& rhs) const
{
    return reinterpret_cast<const uint32_t*>(&src_xid)[0] == reinterpret_cast<const uint32_t*>(&rhs.src_xid)[0] &&
           reinterpret_cast<const uint32_t*>(&src_xid)[1] == reinterpret_cast<const uint32_t*>(&rhs.src_xid)[1] &&
           reinterpret_cast<const uint32_t*>(&src_xid)[2] == reinterpret_cast<const uint32_t*>(&rhs.src_xid)[2] &&
           reinterpret_cast<const uint32_t*>(&src_xid)[3] == reinterpret_cast<const uint32_t*>(&rhs.src_xid)[3] &&
           reinterpret_cast<const uint32_t*>(&src_xid)[4] == reinterpret_cast<const uint32_t*>(&rhs.src_xid)[4] &&
           reinterpret_cast<const uint32_t*>(&src_xid)[5] == reinterpret_cast<const uint32_t*>(&rhs.src_xid)[5] &&
           reinterpret_cast<const uint32_t*>(&dst_xid)[0] == reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[0] &&
           reinterpret_cast<const uint32_t*>(&dst_xid)[1] == reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[1] &&
           reinterpret_cast<const uint32_t*>(&dst_xid)[2] == reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[2] &&
           reinterpret_cast<const uint32_t*>(&dst_xid)[3] == reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[3] &&
           reinterpret_cast<const uint32_t*>(&dst_xid)[4] == reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[4] &&
           reinterpret_cast<const uint32_t*>(&dst_xid)[5] == reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[5];
}

/** @brief Return if the addresses are different. */
inline bool
XIDpair::operator!=(const XIDpair& rhs) const
{
    return reinterpret_cast<const uint32_t*>(&src_xid)[0] != reinterpret_cast<const uint32_t*>(&rhs.src_xid)[0] ||
           reinterpret_cast<const uint32_t*>(&src_xid)[1] != reinterpret_cast<const uint32_t*>(&rhs.src_xid)[1] ||
           reinterpret_cast<const uint32_t*>(&src_xid)[2] != reinterpret_cast<const uint32_t*>(&rhs.src_xid)[2] ||
           reinterpret_cast<const uint32_t*>(&src_xid)[3] != reinterpret_cast<const uint32_t*>(&rhs.src_xid)[3] ||
           reinterpret_cast<const uint32_t*>(&src_xid)[4] != reinterpret_cast<const uint32_t*>(&rhs.src_xid)[4] ||
           reinterpret_cast<const uint32_t*>(&src_xid)[5] != reinterpret_cast<const uint32_t*>(&rhs.src_xid)[5] ||
           reinterpret_cast<const uint32_t*>(&dst_xid)[0] != reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[0] ||
           reinterpret_cast<const uint32_t*>(&dst_xid)[1] != reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[1] ||
           reinterpret_cast<const uint32_t*>(&dst_xid)[2] != reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[2] ||
           reinterpret_cast<const uint32_t*>(&dst_xid)[3] != reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[3] ||
           reinterpret_cast<const uint32_t*>(&dst_xid)[4] != reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[4] ||
           reinterpret_cast<const uint32_t*>(&dst_xid)[5] != reinterpret_cast<const uint32_t*>(&rhs.dst_xid)[5];
}

CLICK_ENDDECLS
#endif
