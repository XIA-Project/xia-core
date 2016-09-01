/*
 * xianetjoin.(cc,hh) --
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
#include "xianetjoin.hh"
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

#define APIPORT           9228
#define XIANETJOINDEVPORT 0
#define XIANETJOINAPIPORT 1
#define XNETJ_BROADCAST_IFACE 255

XIANetJoin::XIANetJoin()
{

}

// no cleanup needed
XIANetJoin::~XIANetJoin()
{

}

// allow users to modify SRC/DST and the frequency of prints
int
XIANetJoin::configure(Vector<String> &conf, ErrorHandler *errh)
{
	// TODO: Pass down number of ports and use for broadcast loop below
    if (cp_va_kparse(conf, this, errh,
				cpEnd) < 0)
        return -1; // error config

    return 0;
}

int
XIANetJoin::initialize(ErrorHandler *)
{
    return 0;
}

void
XIANetJoin::push(int in_port, Packet *p_in)
{
	switch(in_port) {
		case XIANETJOINDEVPORT:
			{
			// Received a packet destined for this host
			DBG("XIANetJoin: Received a packet from XNetJ");
			std::string p_buf;
			p_buf.assign((const char *)p_in->data(), (const char *)p_in->end_data());
			WritablePacket *apiPacket = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
			apiPacket->set_dst_ip_anno(IPAddress("127.0.0.1"));
			SET_DST_PORT_ANNO(apiPacket, htons(APIPORT));
			output(XIANETJOINAPIPORT).push(apiPacket);
			p_in->kill();
			}
			break;
		case XIANETJOINAPIPORT:
			{
			// Received a packet from NetJoin API to be sent on the wire
			// Read outgoing interface from packet but don't strip it
			uint16_t iface = (uint16_t)*(p_in->data());

			// Broadcast the packet on all interfaces, if requested
			if(iface == XNETJ_BROADCAST_IFACE) {
				DBG("XIANetJoin: Broadcasting API packet to XNetjs");
				//TODO: Hardcoded number of ports. Pass down from click conf
				for(int i=0; i<4; i++) {
					Packet *q = p_in->clone();
					SET_XIA_PAINT_ANNO(q, i);
					output(XIANETJOINDEVPORT).push(q);
				}
				p_in->kill();
				return;
			}

			// Or send it to the specifically requested interface
			SET_XIA_PAINT_ANNO(p_in, iface);
			DBG("XIANetJoin: Sending pkt to Iface %d XNetj", iface);

			output(XIANETJOINDEVPORT).push(p_in);
			}
			break;
	};
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIANetJoin)
ELEMENT_MT_SAFE(XIANetJoin)
