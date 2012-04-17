/*
 * xcmppingsource.(cc,hh) -- element that sends xcmp pings to a specified address
 *
 *
 * Copyright 2012 Carnegie Mellon University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <click/config.h>
#include "xcmppingsource.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

// initialize the sequence number
XCMPPingSource::XCMPPingSource()
{
    _seqnum = 0;
}

// no cleanup needed
XCMPPingSource::~XCMPPingSource()
{

}

// allow users to modify SRC/DST and the frequency of prints
int
XCMPPingSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _print_every = 1;

    if (cp_va_kparse(conf, this, errh,
                   "SRC", cpkP+cpkM, cpXIAPath, &_src_path,
                   "DST", cpkP+cpkM, cpXIAPath, &_dst_path,
                   "PRINT_EVERY", 0, cpInteger, &_print_every,
                   cpEnd) < 0)
        return -1; // error config

    return 0;
}

int
XCMPPingSource::initialize(ErrorHandler *)
{
    return 0;
}

// create a xcmp ping packet to send to the network
Packet *
XCMPPingSource::pull(int) {
    // create the ping packet
    WritablePacket *p = Packet::make(256, NULL, 8, 0);

    *(uint8_t*)(p->data() + 0) = 8; // PING
    *(uint8_t*)(p->data() + 1) = 0; // CODE 0

    *(uint16_t*)(p->data() + 2) = 0; // CHECKSUM

    *(uint16_t*)(p->data() + 4) = 0xBEEF; // ID
    *(uint16_t*)(p->data() + 6) = _seqnum; // SEQ_NUM

	// encapsulate the ping packet within an XCMP packet
    XIAHeaderEncap encap;
    encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
    encap.set_dst_path(_dst_path);
    encap.set_src_path(_src_path);

	// print out the packet if requested during setup
	if (_seqnum % _print_every == 0)
		click_chatter("%u: PING sent; client seq = %u\n", Timestamp::now().usecval(), _seqnum);

	_seqnum++;

	return encap.encap(p);
}

// read packets in from the network
void
XCMPPingSource::push(int, Packet *p)
{
    XIAHeader hdr(p);

	// if the packet is not an XCMP packet, discard
    if(hdr.nxt() != CLICK_XIA_NXT_XCMP) { // not XCMP
      p->kill();
      return;
    }

	// what kind of XCMP packet is it?
    const uint8_t *payload = hdr.payload();

    switch (*payload) {
	// we really don't care if we're being ping'd
    case 8: // PING
      click_chatter("ignoring PING at XCMPPingSource\n");
      break;
      
	// this is most likely a response from the host we ping'd
    case 0: // PONG
      click_chatter("%u: PONG received; client seq = %u\n", p->timestamp_anno().usecval(), *(uint16_t *)(hdr.payload()+6));
      break;

	// this is some other XCMP message type
    default:
      click_chatter("invalid message type\n");
      break;
    }

	// kill the packet
    p->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XCMPPingSource)
ELEMENT_MT_SAFE(XCMPPingSource)
