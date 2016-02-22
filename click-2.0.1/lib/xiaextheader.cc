// -*- related-file-name: "../include/click/xiaextheader.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiaextheader.hh>
#include <click/packet_anno.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#include <iostream>
#endif


CLICK_DECLS

/** @file xiaextheader.hh
 */

XIAGenericExtHeader::XIAGenericExtHeader(const XIAGenericExtHeader& r)
	: _hdr(r._hdr), _valid(false), _payload_length(0)
{
	populate_map();
}

XIAGenericExtHeader::XIAGenericExtHeader(const struct click_xia_ext* hdr)
	: _hdr(hdr), _valid(false), _payload_length(0)
{
	populate_map();
}

XIAGenericExtHeader::XIAGenericExtHeader(const Packet* p, uint8_t t)
{
	XIAHeader xiah(p);
	uint8_t nxt = xiah.nxt();
	const struct click_xia_ext* ext_hdr(reinterpret_cast<const struct click_xia_ext*>(xiah.next_header()));
	const uint8_t *phdr = (uint8_t*)ext_hdr;

	//calcalate the size of the actual payload
	// This gives us the size of the payload and any headers that follow the header
	// we are looking for
	int payload_size = xiah.plen();
	payload_size -= ext_hdr->hlen;

    //click_chatter("total payload size = %d this header size = %d adjusted plen = %d", xiah.plen(), ext_hdr->hlen, payload_size);

	// start by assuming that we found the header type we were looking for
	_valid = true;

	if (nxt == t) {
		_hdr = ext_hdr;
		populate_map();

	} else {
//        click_chatter("Looking for Header of type %d found %d\n", t, nxt);
//        click_chatter("Next = %d\n", ext_hdr->nxt);
//        click_chatter("ext_hdr = %p\n", ext_hdr);
//        click_chatter("payload size=%d total:%d\n", payload_size, xiah.plen());

		while (ext_hdr->nxt != t && ext_hdr->nxt != CLICK_XIA_NXT_NO) {

			// walk the header chain
			phdr += ext_hdr->hlen;
			ext_hdr = (const struct click_xia_ext*)phdr;

			//click_chatter("next ext_hdr = %p\n", ext_hdr);

			payload_size -= ext_hdr->hlen;
            //click_chatter("Next = %d\n", ext_hdr->nxt);
            //click_chatter("this header size = %d adjusted plen = %d", ext_hdr->hlen, payload_size);
		}

		if (ext_hdr->nxt == t) {
			phdr += ext_hdr->hlen;
			_hdr = (const struct click_xia_ext*)phdr;
//			click_chatter("_hdr = %p nxt:%d hlen:%d\n", _hdr, _hdr->nxt, _hdr->hlen);

			payload_size -= _hdr->hlen;
//			click_chatter("this header size = %d adjusted plen = %d", _hdr->hlen, payload_size);

			populate_map();

		} else {
			// something horrible happened and we're gonna crash
//			click_chatter("Header %d not found!", t);
			_valid = false;

			payload_size = 0;
			assert(0);
		}
	}

	// save the amout of data following this header so we
	// can get it in other code without having to recompute
	_payload_length = payload_size;
//    click_chatter("type:%d payload_size %d\n", t, payload_size);
}

XIAGenericExtHeader::XIAGenericExtHeader(const Packet* p)
	: _hdr(reinterpret_cast<const struct click_xia_ext*>(XIAHeader(p).next_header()))
{
	populate_map();
}

void
XIAGenericExtHeader::populate_map()
{
	_map.clear();

	const uint8_t* d = _hdr->data;
	const uint8_t* end = reinterpret_cast<const uint8_t*>(_hdr) + _hdr->hlen;

	while (d < end) {
		uint8_t kv_len = *d++;

		if (!kv_len) {
			// hit padding
			break;
		}

		if (d + kv_len > end) {
			click_chatter("invalid kv_len or hlen");
			break;
		}

		uint8_t key = *d;

		String value(reinterpret_cast<const char*>(d + 1), kv_len - 1);
		_map[key] = value;
		d += kv_len;
	}
}

void XIAGenericExtHeader::dump() const
{
	click_chatter("     nxt: %d\n", _hdr->nxt);
	click_chatter("    hlen: %d\n", _hdr->hlen);
	click_chatter("    plen: %d\n", plen());
}

WritableXIAGenericExtHeader::WritableXIAGenericExtHeader(const WritableXIAGenericExtHeader& r)
	: XIAGenericExtHeader(r._hdr)
{
}

WritableXIAGenericExtHeader::WritableXIAGenericExtHeader(struct click_xia_ext* hdr)
	: XIAGenericExtHeader(hdr)
{
}

WritableXIAGenericExtHeader::WritableXIAGenericExtHeader(WritablePacket* p)
	: XIAGenericExtHeader(p)
{
}

XIAGenericExtHeaderEncap::XIAGenericExtHeaderEncap()
{
	const size_t size = sizeof(struct click_xia_ext);
	_hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]); 
	memset(_hdr, 0, size);
	_hdr->nxt = CLICK_XIA_NXT_NO;
	_hdr->hlen = size;
	assert(hlen() == size);
}

XIAGenericExtHeaderEncap::XIAGenericExtHeaderEncap(const XIAGenericExtHeaderEncap& r)
{
	const size_t size = r.hlen();
	_hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]); 
	memcpy(_hdr, r._hdr, size);
	_map = r._map;
	assert(hlen() == size);
}

XIAGenericExtHeaderEncap::~XIAGenericExtHeaderEncap()
{
	delete [] reinterpret_cast<uint8_t*>(_hdr);
	_hdr = NULL;
}

XIAGenericExtHeaderEncap::XIAGenericExtHeaderEncap(const XIAGenericExtHeader& r)
{
	const size_t size = r.hlen();
	_hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]); 
	memcpy(_hdr, r.hdr(), size);
	_map = r.map();
	assert(hlen() == size);
}

const struct click_xia_ext*
XIAGenericExtHeaderEncap::hdr() const
{
	return _hdr;
}

struct click_xia_ext*
XIAGenericExtHeaderEncap::hdr()
{
	return _hdr;
}

const uint8_t&
XIAGenericExtHeaderEncap::hlen() const
{
	return _hdr->hlen;
}

void
XIAGenericExtHeaderEncap::set_nxt(uint8_t nxt)
{
	_hdr->nxt = nxt;
}

void
XIAGenericExtHeaderEncap::update()
{
	size_t size = sizeof(struct click_xia_ext);
	HashTable<uint8_t, String>::const_iterator it = _map.begin();
	size_t count = 0;
	size_t padding = 0;
	for (; it != _map.end(); ++it) {
		if ((*it).second.length() >= 255 - 1) {   // skip too long value
			click_chatter("too long value for key %d", (*it).first);
			continue;
		}
		size_t new_size = size + offsetof(struct click_xia_ext, data) + (*it).second.length();
		if (new_size >= 255) {
			click_chatter("too large key-value map");
			break;
		}
		size = new_size;
		count++;
	}
	if ((size & 3) != 0) {
		padding = 4 - (size & 3);
		size += padding;
	}

	click_xia_ext* new_hdr = reinterpret_cast<struct click_xia_ext*>(new uint8_t[size]);

	// preserve the current header content except key-value map
	memcpy(new_hdr, _hdr, sizeof(struct click_xia_ext));

	// update the new header
	new_hdr->hlen = size;

	uint8_t* d = new_hdr->data;
	for (it = _map.begin(); it != _map.end() && count > 0; ++it, count--) {
		if ((*it).second.length() >= 255 - 1)   // skip too long value
			continue;
		// key-value length
		*d++ = 1 + (*it).second.length();
		// key
		*d++ = (*it).first;
		// value
		memcpy(d, (*it).second.data(), (*it).second.length());
		d += (*it).second.length();
	}
	// padding
	memset(d, 0, padding);

	delete [] reinterpret_cast<uint8_t*>(_hdr);
	_hdr = new_hdr;
}

WritablePacket*
XIAGenericExtHeaderEncap::encap(Packet* p_in) const
{
	size_t len = hlen();    // this call also set _hdr->hlen
	WritablePacket* p = p_in->push(len);
	if (!p)
		return NULL;

	/*
	for (HashTable<uint8_t, String>::const_iterator it = _map.begin(); it != _map.end(); ++it)
		click_chatter("%d %d %02hhx %02hhx\n", (*it).first, (*it).second.length(), (*it).second.c_str()[0], (*it).second.c_str()[1]); 
	*/

	memcpy(p->data(), _hdr, hlen());  // copy the header

	return p;
}

CLICK_ENDDECLS
