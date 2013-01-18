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

#define DEBUG 1

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
XCMP::sendXCMPPacket(Packet *p_in, int type, int code, click_xia_xid *addr) {
    char msg[256];

    XIAHeader hdr(p_in);

    if(type == 0) { // pong
        // copy the data from the ping (ie. we need the seq num and ID,
        // so the sender can figure out what ping our pong refers to)
        memcpy(msg, hdr.payload(), hdr.plen());
    }

    msg[0] = type; 
    msg[1] = code;
		
    // copy the correct route redirect address into the packet if needed
    if(addr)
        memcpy(&msg[4], addr, sizeof(struct click_xia_xid));
		
    // copy in the XIA header and the first 8 bytes of the datagram.
    // strictly speaking, this first 8 bytes don't get us anything,
    // but for the sake of similarity to ICMP we include them.
    memcpy(&msg[4+sizeof(struct click_xia_xid)], hdr.hdr(), hdr.hdr_size()); // copy XIP header
    memcpy(&msg[4+sizeof(struct click_xia_xid)+hdr.hdr_size()], 
           hdr.payload(), 8); // copy first 8 bytes of datagram
		
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
		
    // clear the paint
    if(type == 5) // redirect
        SET_XIA_PAINT_ANNO(p, XIA_PAINT_ANNO(p_in)*-1-TOTAL_SPECIAL_CASES);
    else 
        SET_XIA_PAINT_ANNO(p, DESTINED_FOR_LOCALHOST);

    // send the packet out to the network
    output(0).push(encap.encap(p));

    return;
}


// processes a data packet that should be been sent somewhere else
void
XCMP::processBadForwarding(Packet *p_in) {
    XIAHeader hdr(p_in);
    if(DEBUG)
        click_chatter("%s: %u: %s sent me a packet that should route on its local network (Dst: %s)\n", 
                      _src_path.unparse().c_str(), p_in->timestamp_anno().usecval(), hdr.src_path().unparse().c_str(), 
                      hdr.dst_path().unparse().c_str());

    // get the next hop information stored in the packet annotation
    XID x = p_in->nexthop_neighbor_xid_anno();
    struct click_xia_xid xid_temp;
    xid_temp = x.xid();

    sendXCMPPacket(p_in, 5, 1, &xid_temp); // send a redirect
	   
    if(DEBUG)
        click_chatter("%s: %u: Redirect sent (paint color: %d)\n", _src_path.unparse().c_str(), Timestamp::now().usecval(), XIA_PAINT_ANNO(p_in)*-1-TOTAL_SPECIAL_CASES);

    return;
}

// processes a data packet that was undeliverable
void
XCMP::processUnreachable(Packet *p_in) {
    XIAHeader hdr(p_in);
    if(hdr.nxt() == CLICK_XIA_NXT_XCMP && ((unsigned char *)hdr.payload())[0] == 3) { // can't deliever an unreachable?
        // return immediately to avoid infinite packet generation
        return;
    }

    if(DEBUG)
        click_chatter("%s: %u: Dest (S: %s , D: %s) Unreachable\n", _src_path.unparse().c_str(), 
                      p_in->timestamp_anno().usecval(), hdr.src_path().unparse().c_str(), 
                      hdr.dst_path().unparse().c_str());
      
    sendXCMPPacket(p_in, 3, 1, NULL); // send an undeliverable message
      
    if(DEBUG)
        click_chatter("%s: %u: Dest Unreachable sent to %s\n", _src_path.unparse().c_str(), 
                      Timestamp::now().usecval(), hdr.src_path().unparse().c_str());

    return;
}

// process a data packet that had expired
void
XCMP::processExpired(Packet *p_in) {
    if(DEBUG)
        click_chatter("%s: %u: HLIM Exceeded\n", _src_path.unparse().c_str(), p_in->timestamp_anno().usecval());
    
    sendXCMPPacket(p_in, 11, 0, NULL); // send a TTL expire message
  		
    if(DEBUG)
        click_chatter("%s: %u: TIME EXCEEDED sent\n", _src_path.unparse().c_str(), Timestamp::now().usecval());
		
    return;
}

// receive a ping and send a pong back to the sender
void
XCMP::gotPing(Packet *p_in) {
    XIAHeader hdr(p_in);
    if(DEBUG)
        click_chatter("%s: %u: PING received; client seq = %u\n", _src_path.unparse().c_str(), 
                      p_in->timestamp_anno().usecval(), *(uint16_t*)(hdr.payload() + 6));

    sendXCMPPacket(p_in, 0, 0, NULL);

    /*
    // TODO: What is this?
    // set the dst ip anno for arp
    re = hdr.src_path().unparse();
		

    for(i = 0; i < strlen(re.c_str()); i++)
    if(re.c_str()[i] == 'H') break;

    i=i+36; // get to IP part

    sscanf(&re.c_str()[i], "%2x%2x%2x%2x", (((unsigned char*)&uintip)+0), (((unsigned char*)&uintip)+1), (((unsigned char*)&uintip)+2), (((unsigned char*)&uintip)+3));
    ip = new IPAddress(uintip);

    p->set_anno_u32(Packet::dst_ip_anno_offset, uintip);

    //printf("XID = %s\n", hdr.src_path().unparse().c_str());
    //printf("XID = %s\n", re.c_str());
    //printf("XID = %s, IP=%s\n",&re.c_str()[i],ip->unparse().c_str());

    //click_chatter("src = %s\n", hdr.src_path().unparse().c_str());
    */

    if(DEBUG)
        click_chatter("%s: %u: PONG sent; client seq = %u\n", _src_path.unparse().c_str(), 
                      Timestamp::now().usecval(), *(uint16_t*)(p_in->data() + 6));
}

// got pong, send up
void
XCMP::gotPong(Packet *p_in) {
    XIAHeader hdr(p_in);
    if(DEBUG)
        click_chatter("%s: %u: PONG recieved; client seq = %u\n", _src_path.unparse().c_str(), Timestamp::now().usecval(), *(uint16_t*)(hdr.payload() + 6));
    sendUp(p_in);
}

// got expiry packet, send up
void
XCMP::gotExpired(Packet *p_in) {
    if(DEBUG)
        click_chatter("%s: %u: Received TIME EXCEEDED\n", _src_path.unparse().c_str(), Timestamp::now().usecval());
    sendUp(p_in);
}

// got unreachable packet, send up
void
XCMP::gotUnreachable(Packet *p_in) {
    if(DEBUG)
        click_chatter("%s: %u: Received UNREACHABLE\n", _src_path.unparse().c_str(), Timestamp::now().usecval());
    sendUp(p_in);		
}

// got redirect packet, send up
void
XCMP::gotRedirect(Packet *p_in) {
    if(DEBUG)
        click_chatter("%s: %u: Received REDIRECT\n", _src_path.unparse().c_str(), Timestamp::now().usecval());
    
    XIAHeader hdr(p_in);
    const uint8_t *payload = hdr.payload();
    const uint8_t *pay;
    XID *newroute;
    XIAHeader *badhdr;

    pay = payload;
    newroute = new XID((const struct click_xia_xid &)(pay[4]));
    badhdr = new XIAHeader((const struct click_xia *)(&pay[4+sizeof(struct click_xia_xid)]));
    if(DEBUG)
        click_chatter("%s: %u: REDIRECT INFO: %s told me (%s) that in order to send to %s, I should first send to %s\n",
                      _src_path.unparse().c_str(), Timestamp::now().usecval(), hdr.src_path().unparse().c_str(), 
                      hdr.dst_path().unparse().c_str(), badhdr->dst_path().unparse().c_str(), newroute->unparse().c_str());
		
    delete newroute;
    delete badhdr;

    // paint the packet so the upper level routing process
    // knows to update its table
    SET_XIA_PAINT_ANNO(p_in, 1);
		
    // send the Route Update to Host
    output(1).push(p_in);
}

// process packet and send out proper xcmp message.
// returns false if packet was not flagged as bad in any way.
bool
XCMP::processPacket(Packet *p_in) {
    XIAHeader hdr(p_in);
	// check if this packet is painted for redirection
    if(XIA_PAINT_ANNO(p_in) <= -1*TOTAL_SPECIAL_CASES) { // need to send XCMP REDIRECT
		processBadForwarding(p_in);
        return true;
    }

	// check to see if this packet can't make it to its destination
	// we need to send a DESTINATION XID Unreachable message to the src
    if(XIA_PAINT_ANNO(p_in) == UNREACHABLE) { // need to send a DESTINATION XID UNREACHABLE MESSAGE
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
    XIAHeader hdr(p_in);
    const uint8_t *payload = hdr.payload();

	// what type is it?
    switch (*payload) {

    case 8: // PING
        gotPing(p_in);
        break;
	  
    case 0: // PONG
        gotPong(p_in);
        break;

    case 11: // Time EXCEEDED (ie. TTL expiration)
        gotExpired(p_in);
		break;

	case 3: // XID unreachable
        gotUnreachable(p_in);
		break;
		
	case 5: // redirect
        gotRedirect(p_in);
        break;

    default:
	    // BAD MESSAGE TYPE
        click_chatter("invalid XCMP message type\n");
        break;
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
