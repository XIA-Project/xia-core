/*
 * xnetj.(cc,hh) --
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
#include "xnetj.hh"
#include <clicknet/ether.h>
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xid.hh>
#include <click/xiaheader.hh>
#include <click/xiasecurity.hh>
#include <click/packet_anno.hh>
#include "xlog.hh"

CLICK_DECLS

#define XNETJDEVPORT 0
#define XNETJOINPORT 1

XNetJ::XNetJ()
{

}

// no cleanup needed
XNetJ::~XNetJ()
{

}

// allow users to modify SRC/DST and the frequency of prints
int
XNetJ::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
				"ETH", cpkP + cpkM, cpEtherAddress, &_my_en,
				cpEnd) < 0)
        return -1; // error config

    return 0;
}

int
XNetJ::initialize(ErrorHandler *)
{
    return 0;
}

void
XNetJ::push(int in_port, Packet *p_in)
{
	switch(in_port) {
		case XNETJDEVPORT:
			{
			// Received a packet destined for this host
			uint16_t iface = SRC_PORT_ANNO(p_in);
			DBG("XNetJ: Dev: Interface %d: Got packet", (int)iface);
			click_ether *e = (click_ether *)p_in->data();
			std::string p_buf;

			// Strip ethernet header from packet
			p_buf.assign((const char *)p_in->data()+sizeof(click_ether), (const char *)p_in->end_data());

			// Prepend netj header containing src mac address and iface num
			p_buf.insert(0, (const char *)e->ether_shost, 6);
			// TODO: Confirm that it is OK to blindly copy our address here
			// We do this so if dest was broadcast, we send actual addr up
			p_buf.insert(0, (const char *)_my_en.data(), 6);
			p_buf.insert(0, (const char *)&iface, 2);

			WritablePacket *apiPacket = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
			output(XNETJOINPORT).push(apiPacket);
			}
			break;
		case XNETJOINPORT:
			{

			// Received a packet from NetJoin API to be sent on the wire

			std::string p_buf;

			uint16_t iface = (uint16_t)*p_in->data();

			EtherAddress dest_ether_addr =
				EtherAddress((const unsigned char *)p_in->data()+2);

			p_buf.assign((const char *)p_in->data()+8,
					(const char *)p_in->end_data());

			DBG("XNetJ: Outgoing pkt: Iface: %d (255==ALL):", iface);
			DBG("XNetJ: dest: %s", dest_ether_addr.unparse().c_str());
			DBG("XNetJ: src: %s", _my_en.unparse().c_str());
			DBG("XNetJ: size: %d", p_buf.size());

			WritablePacket *p = WritablePacket::make(1024, p_buf.c_str(),
					p_buf.size(), 0);

			WritablePacket *q = p->push_mac_header(sizeof(click_ether));
			if(!q) {
				ERROR("XNetJ: ERROR: dropping API packet");
				return;
			}

			q->ether_header()->ether_type = htons(ETHERTYPE_XNETJ);
			memcpy(&q->ether_header()->ether_dhost, dest_ether_addr.data(), 6);
			memcpy(&q->ether_header()->ether_shost, _my_en.data(), 6);

			output(XNETJDEVPORT).push(q);
			}
			break;
	};
	// p_in contents were copied to p_buf and sent out as needed
	p_in->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XNetJ)
ELEMENT_MT_SAFE(XNetJ)
