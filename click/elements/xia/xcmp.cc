/*
 * xcmp.(cc,hh) -- element that handles sending and receiving xcmp messages
 *
 *
 * Copyright 2012 Carnegie Mellon University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <click/config.h>
#include "xcmp.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
#include <click/packet_anno.hh>
#include "xlog.hh"
CLICK_DECLS

#define MAX_PACKET		1500
#define NUM_EMPTY_BYTES	4	// # of empty bytes in a ttl or unreachable header
#define NUM_TIME_BYTES	8	// # of bytes to reserver for timing in ping/pong headers
#define NUM_ORIG_BYTES	8	// # of bytes of original pkt to append to ttl/unreach packets
#define SEQ_OFF			6	// offset of the sequence # in ping/pong pkts
// user must specify the XIAPath (ie. RE AD0 HID0) of the host
// that this element resides on
int
XCMP::configure(Vector<String> &conf, ErrorHandler *errh)
{
	if (cp_va_kparse(conf, this, errh, cpEnd) < 0) {
		return -1;
	}
	return 0;
}

int
XCMP::initialize(ErrorHandler *)
{
	return 0;
}

enum {DAG, HID};

int XCMP::write_param(const String &conf, Element *e, void *vparam, ErrorHandler *errh)
{
	XCMP *f = static_cast<XCMP *>(e);
	switch(reinterpret_cast<intptr_t>(vparam)) {
	case DAG:
	{
		XIAPath dag;
		if (cp_va_kparse(conf, f, errh, "ADDR", cpkP + cpkM, cpXIAPath, &dag, cpEnd) < 0) {
			return -1;
		}
		f->_src_path = dag;
		DBG("XCMP: DAG is now %s", f->_src_path.unparse().c_str());
		break;

	}
	case HID:
	{
		XID hid;
		if (cp_va_kparse(conf, f, errh, "HID", cpkP + cpkM, cpXID, &hid, cpEnd) < 0) {
			return -1;
		}
		f->_hid = hid;
		DBG("XCMP: HID assigned: %s", hid.unparse().c_str());
		break;
	}
	default:
		break;
	}
	return 0;
}

void XCMP::add_handlers()
{
	add_write_handler("dag", write_param, (void *)DAG);
	add_write_handler("hid", write_param, (void *)HID);
}


const char *XCMP::unreachStr(int code)
{
	switch(code) {
		case XCMP_UNREACH_NET:
			return "NETWORK";
		case XCMP_UNREACH_HOST:
			return "HOST";
			break;
		case XCMP_UNREACH_INTENT:
			return "INTENT";
		default:
			return "UNKNOWN";
	}
}


// sends a packet up to the application layer
void
XCMP::sendUp(Packet *p_in) {
	// clear the paint
	SET_XIA_PAINT_ANNO(p_in, 0);

	// send packet up to local host
	output(1).push(p_in);

	return;
}

// create and send XCMP packet with the following params. _addr_ is used for redirects.
void
XCMP::sendXCMPPacket(const Packet *p_in, int type, int code, XIAPath *src) {

	const XIAHeader hdr(p_in);
	char msg[MAX_PACKET];
	size_t xlen;

	switch(type) {
		case XCMP_ECHOREPLY:
			// send back the same size packet
			xlen = hdr.plen();

			// copy the data from the ping (ie. we need the seq num and ID,
			// so the sender can figure out what ping our pong refers to)
			memcpy(msg, hdr.payload(), xlen);
			break;

		case XCMP_UNREACH:
		case XCMP_TIMXCEED:
		default:
			xlen = sizeof(struct click_xia_xcmp);

			memset(msg + xlen, 0, NUM_EMPTY_BYTES);	// first 4 bytes after xcmp header are 0
			xlen += NUM_EMPTY_BYTES;
			memcpy(msg + xlen, hdr.hdr(), hdr.hdr_size());
			xlen += hdr.hdr_size();

			// copy 1st 8 bytes of payload as is done in ICMP
			memcpy(msg + xlen, hdr.payload(), NUM_ORIG_BYTES);
			xlen += NUM_ORIG_BYTES;
			break;
	}

	struct click_xia_xcmp *xcmph = reinterpret_cast<struct click_xia_xcmp *>(msg);
	xcmph->type = type;
	xcmph->code = code;
	xcmph->cksum = 0;
	// update the checksum
	uint16_t checksum = in_cksum((u_short *)msg, xlen);
	xcmph->cksum = checksum;

	// create a packet to store the message in
	WritablePacket *p = Packet::make(256, msg, xlen, 0);

	// encapsulate the packet within XCMP
	XIAHeaderEncap encap;
	encap.set_nxt(CLICK_XIA_NXT_XCMP);
	encap.set_dst_path(hdr.src_path());
	if (src) {
		encap.set_src_path(*src);
	} else {
		encap.set_src_path(hdr.dst_path());
	}

	// clear the paint
	SET_XIA_PAINT_ANNO(p, DESTINED_FOR_LOCALHOST);

	WritablePacket *pkt = encap.encap(p);

	// send the packet out to the network
	output(0).push(pkt);
}

// processes a data packet that was undeliverable
void
XCMP::processUnreachable(Packet *p_in)
{
	XIAHeader hdr(p_in);
	int code = XCMP_UNREACH_UNSPECIFIED;

	// can't deliever an unreachable?
	if (hdr.nxt() == CLICK_XIA_NXT_XCMP && ((unsigned char *)hdr.payload())[0] == XCMP_UNREACH) {
		// return immediately to avoid infinite packet generation
		return;
	}

	XIAPath dst_path;
	try {
		dst_path = hdr.dst_path();
	} catch (std::range_error &e) {
		WARN("XCMP::processUnreachable ERROR Invalid dst path in pkt.\n");
		return;
	}
	String broadcast_xid(BHID);
	XID bcast_xid;
	bcast_xid.parse(broadcast_xid);

	if (!dst_path.is_valid()) {
		INFO("xcmp: discarding invalid path. %s\n", dst_path.unparse().c_str());
		INFO("Sending Unreachable from %s\n   to %s\n   for %s\n",
			 _src_path.unparse().c_str(),
			hdr.src_path().unparse().c_str(), hdr.dst_path().unparse().c_str());
		return;
	}

	// don't send undeliverables back to broadcast packets
	if (dst_path.intent_hid_str().compare(BHID)) {

		return;
	}

	// the xia_path code seems to discard the visited values, so we need to go
	// into the dags directly
	const struct click_xia *h = hdr.hdr();
	unsigned dnodes = h->dnode;
	int8_t last = h->last;
	const struct click_xia_xid_node *n = &h->node[0];
	int bad_node = -1;

	if (last == -1) {
		n += (dnodes - 1);
	} else {
		n += last;
	}

	for (int i = 0; i < CLICK_XIA_XID_EDGE_NUM; i++) {
		if (!n->edge[i].visited) {
			bad_node = n->edge[i].idx;
			break;
		}
	}

	if (bad_node >= 0) {
		unsigned t = htonl(h->node[bad_node].xid.type);
		switch (t) {
			case CLICK_XIA_XID_TYPE_AD:
				code = XCMP_UNREACH_NET;
				break;
			case CLICK_XIA_XID_TYPE_HID:
				code = XCMP_UNREACH_HOST;
				break;
			default:
				code = XCMP_UNREACH_INTENT;
		}
	} else {
		code = XCMP_UNREACH_UNSPECIFIED;
	}

	// send an undeliverable message
	sendXCMPPacket(p_in, XCMP_UNREACH, code, &_src_path);
	INFO("Sent %s Unreachable from %s\n   to %s\n   for %s\n",
		unreachStr(code), _src_path.unparse().c_str(),
		hdr.src_path().unparse().c_str(), hdr.dst_path().unparse().c_str());
}

// process a data packet that had expired
void
XCMP::processExpired(Packet *p_in) {
	XIAHeader hdr(p_in);

	sendXCMPPacket(p_in, XCMP_TIMXCEED, XCMP_TIMXCEED_TRANSIT, &_src_path);
	INFO("Sent HLIM Exceeded from %s\n   for %s\n   => %s\n",
		_src_path.unparse().c_str(),
		hdr.src_path().unparse().c_str(), hdr.dst_path().unparse().c_str());
	return;
}

// process packet and send out proper xcmp message.
// returns false if packet was not flagged as bad in any way.
bool
XCMP::processPacket(Packet *p_in) {
	XIAHeader hdr(p_in);

	// check to see if this packet can't make it to its destination
	// we need to send a DESTINATION XID Unreachable message to the src
	if(XIA_PAINT_ANNO(p_in) == UNREACHABLE) {
		processUnreachable(p_in);
		return true;
	}

	// check to see if this packet hop-limit has expired
	if(hdr.hlim() <= 1) { // need to send a TIME EXCEEDED MESSAGE
		processExpired(p_in);
		return true;
	}

	return false;
}

// got an xcmp packet, need to either send up or respond with a different xcmp message
void
XCMP::gotXCMPPacket(Packet *p_in) {
	char pload[1500];
	XIAHeader hdr(p_in);

	// have to work around const qualifiers
	memcpy(pload, hdr.payload(), hdr.plen());

	struct click_xia_xcmp *xcmph = reinterpret_cast<struct click_xia_xcmp *>(pload);
	uint16_t cksum = xcmph->cksum;
	xcmph->cksum = 0;
	uint16_t computed_cksum = in_cksum((u_short *)pload, hdr.plen());

	if (cksum != computed_cksum) {
		WARN("computed checksum (0x%04x) != reported checksum (0x%04x), discarding packet", computed_cksum, cksum);
		return;
	}

	if (xcmph->type == XCMP_ECHO) {
		// PING
		uint16_t sn = ntohs(*(uint16_t*)(hdr.payload() + SEQ_OFF));

		// src = ping sender, dest = us
		INFO("PING #%u received: %s\n      => %s\n", sn,
			hdr.src_path().unparse().c_str(), hdr.dst_path().unparse().c_str());

		// src = us, dest = original sender
		INFO("PONG #%u sent: %s\n      => %s\n", sn,
			hdr.dst_path().unparse().c_str(), hdr.src_path().unparse().c_str());

		sendXCMPPacket(p_in, XCMP_ECHOREPLY, 0, NULL);

	} else if (xcmph->type == XCMP_ECHOREPLY) {
		// PONG
		uint16_t sn = ntohs(*(uint16_t*)(hdr.payload() + SEQ_OFF));
		// src  = them, dest = us
		INFO("PONG #%d recieved: %s\n      => %s\n", sn,
			hdr.src_path().unparse().c_str(), hdr.dst_path().unparse().c_str());
		sendUp(p_in);

	} else {
		// get the header out of the data section of the packet
		const struct click_xia* h = (const struct click_xia*)(hdr.payload() +
									sizeof(struct click_xia_xcmp) + NUM_EMPTY_BYTES);
		XIAHeader hresp(h);

		if (xcmph->type == XCMP_TIMXCEED) {
			// Time EXCEEDED (ie. TTL expiration)
			INFO("TTL Exceeded from %s\n      in response to %s\n      => %s\n",
				hdr.src_path().unparse().c_str(),
				hresp.src_path().unparse().c_str(), hresp.dst_path().unparse().c_str());
			sendUp(p_in);

		} else if (xcmph->type == XCMP_UNREACH) {
			// XID unreachable

			INFO("Received %s Unreachable from %s\n      in response to %s\n      => %s\n",
				unreachStr(xcmph->code), hdr.src_path().unparse().c_str(),
				hresp.src_path().unparse().c_str(), hresp.dst_path().unparse().c_str());
			sendUp(p_in);

		} else {
			// BAD MESSAGE TYPE
			WARN("Invalid XCMP message type\n", xcmph->type);
		}
	}
}

// run when reacting to a packet received. decides what to
// do based on the type of packet recieved
void
XCMP::push(int, Packet *p_in)
{
	XIAHeader hdr(p_in);

	if(!processPacket(p_in) && hdr.nxt() == CLICK_XIA_NXT_XCMP) {
		gotXCMPPacket(p_in);
	}

	// kill the incoming packet as we've finished processing it.
	p_in->kill();
}


// from the original PING source
/*
 *					  I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
u_short XCMP::in_cksum(u_short *addr, int len) {
	register int nleft = len;
	register u_short *w = addr;
	register u_short answer;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while( nleft > 1 ) {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if( nleft == 1 ) {
		u_short u = 0;

		*(u_char *)(&u) = *(u_char *)w ;
		sum += u;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);					/* add carry */
	answer = ~sum;						/* truncate to 16 bits */
	return answer;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XCMP)
ELEMENT_MT_SAFE(XCMP)
