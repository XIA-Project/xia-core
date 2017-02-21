/*
 * checkxiaheader.{cc,hh} -- simple XIP header sanity test
 */

#include <click/config.h>
#include "checkxiaheader.hh"
#include <click/confparse.hh>
#include <clicknet/xia.h>
#include <click/xiaheader.hh>
#include "xlog.hh"
CLICK_DECLS

int CheckXIAHeader::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_offset = 0;
	return cp_va_kparse(conf, this, errh, "OFFSET", cpkP, cpUnsigned, &_offset, cpEnd);
}

bool CheckXIAHeader::is_valid_xia_header(Packet *p)
{
	uint32_t len = p->length();
	const char *msg = NULL;

	if (len < sizeof(struct click_xia)) {
		// it's not even big enough for the fixed fields in the xia header
		msg = "Malformed XIP Packet: header truncated";

	} else {
		const click_xia *xiah = reinterpret_cast<const click_xia *>(p->data() + _offset);

		uint32_t hlen = XIAHeader::hdr_size(xiah->dnode + xiah->snode);
		uint16_t plen = ntohs(xiah->plen);

		if (len != (hlen + plen)) {
			// packet length is wrong

			if (len > (hlen + plen)) {
				// FIXME: should we truncate instead??
				msg = "Malformed XIP packet, oversized";
			} else if (len < hlen) {
				msg = "Malformed XIP packet, address truncated";
			} else {
				msg = "Malformed XIP packet,data truncated";
			}

		} else if ((xiah->dnode > CLICK_XIA_ADDR_MAX_NODES) ||
				   (xiah->snode > CLICK_XIA_ADDR_MAX_NODES)) {
			msg = "Malformed XIP packet : invalid DAG";
		}
	}

	if (msg) {
		WARN(msg);
		return false;
	}

	return true;
}

Packet *
CheckXIAHeader::simple_action(Packet *p)
{
	if (!is_valid_xia_header(p)) {
		p->kill();
		return NULL;
	}
	return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(CheckXIAHeader)
ELEMENT_MT_SAFE(CheckXIAHeader)
