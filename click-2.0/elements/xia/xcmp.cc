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
 *    http://www.apache.org/licenses/LICENSE-2.0
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
CLICK_DECLS

#define DEBUG 0

// no initialization needed
XCMP::XCMP()
{

}

// no cleanup needed
XCMP::~XCMP()
{

}

// user must specify the XIAPath (ie. RE AD0 HID0) of the host 
// that this element resides on
int
XCMP::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
                   "SRC", cpkP+cpkM, cpXIAPath, &_src_path,
                   cpEnd) < 0)
        return -1;

    return 0;
}

int
XCMP::initialize(ErrorHandler *)
{
    return 0;
}

// run when reacting to a packet received. decides what to 
// do based on the type of packet recieved
void
XCMP::push(int, Packet *p_in)
{
    XIAHeader hdr(p_in);
    const uint8_t *payload = hdr.payload();

	//if(DEBUG)
	//  click_chatter("XCMP: paint = %d\n",p_in->anno_u8(PAINT_ANNO_OFFSET));

	// check if this packet is painted for redirection
    if(p_in->anno_u8(PAINT_ANNO_OFFSET) >= REDIRECT_BASE) { // need to send XCMP REDIRECT
        if(DEBUG)
		    click_chatter("%s: %u: %s sent me a packet that should route on its local network (Dst: %s)\n", _src_path.unparse().c_str(),p_in->timestamp_anno().usecval(),hdr.src_path().unparse().c_str(),hdr.dst_path().unparse().c_str());
      
		// create the XCMP Redirect Message
		char msg[256];
		msg[0] = 5; // Redirect (type)
		msg[1] = 1; // Redirect for host (code)

		// get the next hop information stored in the packet annotation
		XID x = p_in->nexthop_neighbor_xid_anno();
		struct click_xia_xid xid_temp;
		xid_temp = x.xid();
		
		// copy the correct route redirect address into the packet
		memcpy(&msg[4], &xid_temp, sizeof(struct click_xia_xid));
		
		// copy in the XIA header and the first 8 bytes of the datagram.
		// strictly speaking, this first 8 bytes don't get us anything,
		// but for the sake of similarity to ICMP we include them.
		memcpy(&msg[4+sizeof(struct click_xia_xid)], hdr.hdr(), hdr.hdr_size()); // copy XIP header
		memcpy(&msg[4+sizeof(struct click_xia_xid)+hdr.hdr_size()], 
			   hdr.payload(), 8); // copy first 8 bytes of datagram
		
		// recompute the checksum
		uint16_t checksum = in_cksum((u_short *)msg, hdr.hdr_size()+16);
		memcpy(&msg[2], &checksum, 2);
		
		// create the actual redirect packet
		WritablePacket *p = Packet::make(256, msg, 4+sizeof(struct click_xia_xid)+hdr.hdr_size()+8, 0);
		
		// encap this in XCMP
		XIAHeaderEncap encap;
		encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
		encap.set_dst_path(hdr.src_path());
		encap.set_src_path(_src_path);
		
		if(DEBUG)
		    click_chatter("%s: %u: Redirect sent\n", _src_path.unparse().c_str(),Timestamp::now().usecval());
		
		// paint the packet so the route engine knows where to send it
		p->set_anno_u8(PAINT_ANNO_OFFSET,p_in->anno_u8(PAINT_ANNO_OFFSET)-REDIRECT_BASE);

		// if(DEBUG)
		//    click_chatter("%s: %u: Redirect paint color: %d\n", _src_path.unparse().c_str(),Timestamp::now().usecval(),p->anno_u8(PAINT_ANNO_OFFSET));

		// send this out to the network, kill the packet and exit
		output(0).push(encap.encap(p));
		p_in->kill();
		return;
    }

	// check to see if this packet can't make it to its destination
	// we need to send a DESTINATION XID Unreachable message to the src
    if(p_in->anno_u8(PAINT_ANNO_OFFSET) == XARP_TIMEOUT) { // need to send a DESTINATION XID UNREACHABLE MESSAGE
	    if(hdr.nxt() == CLICK_XIA_NXT_XCMP && ((unsigned char *)hdr.payload())[0] == 3) { // can't deliever an unreachable?
		    // drop this packet to avoid infinite packet generation
		    p_in->kill();
			return;
		}

	    if(DEBUG)
		  click_chatter("%s: %u: Dest (S: %s , D: %s) Unreachable\n", _src_path.unparse().c_str(),p_in->timestamp_anno().usecval(),hdr.src_path().unparse().c_str(),hdr.dst_path().unparse().c_str());
      
		// create a Destination XID Unreachable message
		char msg[256];
		msg[0] = 3; // Dest Unreachable (type)
		msg[1] = 1; // Host (??) unreachable (code)
		
		// copy in the XIA header and the first 8 bytes of the datagram.
		// strictly speaking, this first 8 bytes don't get us anything,
		// but for the sake of similarity to ICMP we include them.
		memcpy(&msg[8], hdr.hdr(), hdr.hdr_size()); // copy XIP header
		memcpy(&msg[8+hdr.hdr_size()], hdr.payload(), 8); // copy first 8 bytes of datagram
		
		// recompute the checksum
		uint16_t checksum = in_cksum((u_short *)msg, hdr.hdr_size()+16);
		memcpy(&msg[2], &checksum, 2);
      
		// create the actual xid unreachable packet
		WritablePacket *p = Packet::make(256, msg, 4+sizeof(struct click_xia_xid)+hdr.hdr_size()+8, 0);
		
		// encap this in XCMP
		XIAHeaderEncap encap;
		encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
		encap.set_dst_path(hdr.src_path());
		encap.set_src_path(_src_path);
      
		if(DEBUG)
		  click_chatter("%s: %u: Dest Unreachable sent to %s\n", _src_path.unparse().c_str(),Timestamp::now().usecval(), hdr.src_path().unparse().c_str());
      
		// clear the paint
		p->set_anno_u8(PAINT_ANNO_OFFSET,0);

		// send this out to the network, kill the packet, and exit
		output(0).push(encap.encap(p));
		p_in->kill();
		return;
    }

	// check to see if this packet hop-limit has expired
    if(hdr.hlim() <= 1) { // need to send a TIME EXCEEDED MESSAGE
	    if(DEBUG)
		    click_chatter("%s: %u: HLIM Exceeded\n", _src_path.unparse().c_str(),p_in->timestamp_anno().usecval());
      
		// create an xcmp message stating time exceeded
		char msg[256];
		msg[0] = 11; // TIME EXCEEDED (type)
		msg[1] = 0; // TTL exceeded in transit (code)
		
		// copy in the XIA header and the first 8 bytes of the datagram.
		// strictly speaking, this first 8 bytes don't get us anything,
		// but for the sake of similarity to ICMP we include them.
		memcpy(&msg[8], hdr.hdr(), hdr.hdr_size()); // copy XIP header
		memcpy(&msg[8+hdr.hdr_size()], hdr.payload(), 8); // copy first 8 bytes of datagram
		
		// update the checksum
		uint16_t checksum = in_cksum((u_short *)msg, hdr.hdr_size()+16);
		memcpy(&msg[2], &checksum, 2);
		
		// create a packet to store the message in
		WritablePacket *p = Packet::make(256, msg, 4+sizeof(struct click_xia_xid)+hdr.hdr_size()+8, 0);
		
		// encapsulate the packet within XCMP
		XIAHeaderEncap encap;
		encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
		encap.set_dst_path(hdr.src_path());
		encap.set_src_path(_src_path);
		
		if(DEBUG)
            click_chatter("%s: %u: TIME EXCEEDED sent\n", _src_path.unparse().c_str(),Timestamp::now().usecval());
		
		// send the packet out, kill the original packet, and exit
		output(0).push(encap.encap(p));
		p_in->kill();
		return;
    }


	// now we know that this packet is not expired
	// check if the packet is even XCMP related
    if(hdr.nxt() != CLICK_XIA_NXT_XCMP) { // not XCMP
        p_in->kill();
		return;
    }

	// now we know the packet is an XCMP packet

	// we'll need these vars within the switch statement
	WritablePacket *p;
	XIAHeaderEncap encap;
	uint16_t checksum;

	const uint8_t *pay;
	XID *newroute;
	XIAHeader *badhdr;
	

	// what type is it?
    switch (*payload) {

    case 8: // PING
	    if(DEBUG)
            click_chatter("%s: %u: PING received; client seq = %u\n", _src_path.unparse().c_str(),p_in->timestamp_anno().usecval(), *(uint16_t*)(payload + 6));

		// we need to send back a pong
		// create a pong message
	    char pong[256];

		// copy the data from the ping (ie. we need the seq num and ID,
		// so the sender can figure out what ping our pong refers to)
	    memcpy(pong, hdr.payload(), hdr.plen());

		// zero the checksum
	    pong[2] = 0;
	    pong[3] = 0;

	    // set the type
	    pong[0] = 0; // PONG

		// recompute the checksum
	    checksum = in_cksum((u_short *)pong, hdr.plen());
	    memcpy(&pong[2], &checksum, 2);

		// create a packet for the pong
	    p = Packet::make(256, pong, hdr.plen(), 0);
            
		// encap the pong into an XCMP packet
		// taking care to set the destination to the source
		// of the ping packet
		encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
		encap.set_dst_path(hdr.src_path());
		encap.set_src_path(_src_path);

		if(DEBUG)
		    click_chatter("%s: %u: PONG sent; client seq = %u\n", _src_path.unparse().c_str(),Timestamp::now().usecval(), *(uint16_t*)(p->data() + 6));
		
		// send out the pong to the network
		output(0).push(encap.encap(p));
		break;
	  
    case 0: // PONG
	    if(DEBUG)
		    click_chatter("%s: %u: PONG recieved; client seq = %u\n", _src_path.unparse().c_str(), Timestamp::now().usecval(), *(uint16_t*)(payload + 6));

		// clear the paint
		p_in->set_anno_u8(PAINT_ANNO_OFFSET,0);

		// send pong up to local host
		output(1).push(p_in);
        break;

    case 11: // Time EXCEEDED (ie. TTL expiration)
	    if(DEBUG)
		    click_chatter("%s: %u: Received TIME EXCEEDED\n", _src_path.unparse().c_str(), Timestamp::now().usecval());

		// clear the paint
		p_in->set_anno_u8(PAINT_ANNO_OFFSET,0);

		// send Time EXCEEDED up to local host
		output(1).push(p_in);
		break;

	case 3: // XID unreachable
	    if(DEBUG)
		    click_chatter("%s: %u: Received UNREACHABLE\n", _src_path.unparse().c_str(), Timestamp::now().usecval());
		
		// clear the paint
		p_in->set_anno_u8(PAINT_ANNO_OFFSET,0);
		
		// send the unreachable up to local host
		output(1).push(p_in);
		break;
		
	case 5: // redirect
	    if(DEBUG)
		    click_chatter("%s: %u: Received REDIRECT\n", _src_path.unparse().c_str(), Timestamp::now().usecval());
    
		//XIAHeader hdr(p_in);
		//const uint8_t *payload = hdr.payload();
		//const uint8_t *pay;
		//XID *newroute;
		//XIAHeader *badhdr;

		pay = payload;
		newroute = new XID((const struct click_xia_xid &)(pay[4]));
		badhdr = new XIAHeader((const struct click_xia *)(&pay[4+sizeof(struct click_xia_xid)]));
		if(DEBUG)
		    click_chatter("%s: %u: REDIRECT INFO: %s told me (%s) that in order to send to %s, I should first send to %s\n",_src_path.unparse().c_str(),Timestamp::now().usecval(),hdr.src_path().unparse().c_str(),hdr.dst_path().unparse().c_str(),badhdr->dst_path().unparse().c_str(),newroute->unparse().c_str());
		
		delete newroute;
		delete badhdr;

		// paint the packet so the upper level routing process
		// knows to update its table
		p_in->set_anno_u8(PAINT_ANNO_OFFSET,1);
		
		// send the Route Update to Host
		output(1).push(p_in);
		break;

    default:
	    // BAD MESSAGE TYPE
        click_chatter("invalid message type\n");
        break;
    }

	// kill the incoming packet as we've finished
	// processing it.
    p_in->kill();
}


// from the original PING source
/*
 *                      I N _ C K S U M
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
  while( nleft > 1 )  {
    //printf("%hu ",*w);
    sum += *w++;
    nleft -= 2;
  }
  //printf("\n");

  /* mop up an odd byte, if necessary */
  if( nleft == 1 ) {
    u_short u = 0;

    *(u_char *)(&u) = *(u_char *)w ;
    sum += u;
  }

  /*
   * add back carry outs from top 16 bits to low 16 bits
   */
  sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
  sum += (sum >> 16);                     /* add carry */
  answer = ~sum;                          /* truncate to 16 bits */
  return (answer);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XCMP)
ELEMENT_MT_SAFE(XCMP)
