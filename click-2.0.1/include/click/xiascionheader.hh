// -*- c-basic-offset: 4; related-file-name: "../../lib/xiaextheader.cc" -*-
#ifndef CLICK_SCIONHEADER_HH
#define CLICK_SCIONHEADER_HH
#include <click/string.hh>
#include <click/glue.hh>
#include <clicknet/xia.h>
#include <click/packet.hh>
#include <click/hashtable.hh>
#include <click/xiaheader.hh>
#include <click/xiaextheader.hh>

CLICK_DECLS

class ScionHeaderEncap;

class ScionHeader : public XIAGenericExtHeader
{
public:
	ScionHeader(const struct click_xia_ext* hdr): XIAGenericExtHeader(hdr) {};
	ScionHeader(const Packet* p): XIAGenericExtHeader(p) {};

	uint8_t opaque_field() { if (!exists(OPAQUE_FIELD)) return 0 ; return *(const uint8_t*)_map[OPAQUE_FIELD].data();};  
	uint16_t length() { if (!exists(LENGTH)) return 0; return *(const uint16_t*)_map[LENGTH].data();};  

	bool exists(uint8_t key) { return (_map.find(key)!=_map.end()); }

	enum { OPAQUE_FIELD, LENGTH }; 
};

class ScionHeaderEncap : public XIAGenericExtHeaderEncap
{
public:
	ScionHeaderEncap(const uint8_t *opaque_field, uint8_t length);

	static ScionHeaderEncap* MakeScionHeader(const uint8_t *opaque_field, uint8_t length) { return new ScionHeaderEncap(opaque_field, length); };
};

CLICK_ENDDECLS
#endif
