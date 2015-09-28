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

CLICK_DECLS

#define APIPORT           9228
#define XIANETJOINAPIPORT 0
#define XIANETJOINDEVPORT 1

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
    if (cp_va_kparse(conf, this, errh,
				"ETH", cpkP + cpkM, cpEtherAddress, &_my_en,
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
		case XIANETJOINAPIPORT:
			{
			// Received a packet from NetJoin API to be sent on the wire
			std::string p_buf;
			p_buf.assign((const char *)p_in->data(), (const char *)p_in->end_data());
			click_chatter("XIANetJoin: API: %s.", p_buf.c_str());

			WritablePacket *q = p_in->push_mac_header(sizeof(click_ether));
			if(!q) {
				click_chatter("XIANetJoin: ERROR: dropping API packet");
				return;
			}
			q->ether_header()->ether_type = htons(ETHERTYPE_XNETJ);
			EtherAddress *dst_eth = reinterpret_cast<EtherAddress *>(q->ether_header()->ether_dhost);
			memset(dst_eth, 0xff, 6);
			memcpy(&q->ether_header()->ether_shost, _my_en.data(), 6);
			output(XIANETJOINDEVPORT).push(q);
			/*
			WritablePacket *reply = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
			reply->set_dst_ip_anno(IPAddress("127.0.0.1"));
			SET_DST_PORT_ANNO(reply, htons(9228));
			// Just forward the packet back to API
			output(0).push(reply);
			*/
			}
			break;
		case XIANETJOINDEVPORT:
			{
			// Received a packet destined for this host
			click_chatter("XIANetJoin: Dev: Received a packet from network");
			click_ether *e = (click_ether *)p_in->data();
			std::string p_buf;
			p_buf.assign((const char *)p_in->data()+sizeof(click_ether), (const char *)p_in->end_data());
			WritablePacket *apiPacket = WritablePacket::make(256, p_buf.c_str(), p_buf.size(), 0);
			apiPacket->set_dst_ip_anno(IPAddress("127.0.0.1"));
			SET_DST_PORT_ANNO(apiPacket, htons(APIPORT));
			output(XIANETJOINAPIPORT).push(apiPacket);
			}
			break;
	};
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIANetJoin)
ELEMENT_MT_SAFE(XIANetJoin)
