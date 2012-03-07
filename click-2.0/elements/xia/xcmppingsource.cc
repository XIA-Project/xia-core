#include <click/config.h>
#include "xcmppingsource.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XCMPPingSource::XCMPPingSource()
{
    _count = 0;
}

XCMPPingSource::~XCMPPingSource()
{
}

int
XCMPPingSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
	_print_every = 1;

    if (cp_va_kparse(conf, this, errh,
                   "SRC", cpkP+cpkM, cpXIAPath, &_src_path,
                   "DST", cpkP+cpkM, cpXIAPath, &_dst_path,
                   "PRINT_EVERY", 0, cpInteger, &_print_every,
                   cpEnd) < 0)
        return -1;

    return 0;
}

int
XCMPPingSource::initialize(ErrorHandler *)
{
    return 0;
}

Packet *
XCMPPingSource::pull(int) {
    WritablePacket *p = Packet::make(256, NULL, 8, 0);

    *(uint8_t*)(p->data() + 0) = 8; // PING
    *(uint8_t*)(p->data() + 1) = 0; // CODE 0

    *(uint16_t*)(p->data() + 2) = 0; // CHECKSUM

    *(uint16_t*)(p->data() + 4) = 0xBEEF; // ID
    *(uint16_t*)(p->data() + 6) = _count; // SEQ_NUM

    XIAHeaderEncap encap;
    encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
    encap.set_dst_path(_dst_path);
    encap.set_src_path(_src_path);

	if (_count % _print_every == 0)
		click_chatter("%u: PING sent; client seq = %u\n", Timestamp::now().usecval(), _count);

	_count++;

	return encap.encap(p);
}

void
XCMPPingSource::push(int, Packet *p)
{
    XIAHeader hdr(p);

    if(hdr.nxt() != CLICK_XIA_NXT_XCMP) { // not XCMP
      p->kill();
      return;
    }

    const uint8_t *payload = hdr.payload();

    switch (*payload) {
    case 8: // PING
      click_chatter("ignoring PING at XCMPPingSource\n");
      break;
      
    case 0: // PONG
      click_chatter("%u: PONG received; client seq = %u\n", p->timestamp_anno().usecval(), *(uint16_t *)(hdr.payload()+6));
      break;

    default:
      click_chatter("invalid message type\n");
      break;
    }

    p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XCMPPingSource)
ELEMENT_MT_SAFE(XCMPPingSource)
