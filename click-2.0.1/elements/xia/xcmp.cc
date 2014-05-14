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
XCMP::sendXCMPPacket(const Packet *p_in, int type, int code, click_xia_xid *lastaddr, click_xia_xid *nxthop, XIAPath *src) {

    const XIAHeader hdr(p_in);
    const size_t xs = sizeof(struct click_xia_xcmp);
    size_t xlen = xs+sizeof(struct click_xia_xid)*2+hdr.hdr_size()+8;

    // make enough room for payload
    if (type==0 && xlen< xs+hdr.plen()) 
        xlen = xs+hdr.plen();

    assert(xlen<1500);
    char* msg = new char[xlen];

    if(type == 0) { // pong
        // copy the data from the ping (ie. we need the seq num and ID,
        // so the sender can figure out what ping our pong refers to)
        memcpy(msg, hdr.payload(), hdr.plen());
    }
 
    // copy the correct route redirect address into the packet if needed
    if(lastaddr && nxthop) {
	memcpy(&msg[xs], lastaddr, sizeof(struct click_xia_xid));
	memcpy(&msg[xs+sizeof(struct click_xia_xid)], nxthop, sizeof(struct click_xia_xid));
    }

    // copy in the XIA header and the first 8 bytes of the datagram.
    // strictly speaking, this first 8 bytes don't get us anything,
    // but for the sake of similarity to ICMP we include them.
    memcpy(&msg[xs+sizeof(struct click_xia_xid)*2], hdr.hdr(), hdr.hdr_size()); // copy XIP header
    memcpy(&msg[xs+sizeof(struct click_xia_xid)*2+hdr.hdr_size()], 
	       hdr.payload(), 8); // copy first 8 bytes of datagram

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
    encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
    encap.set_dst_path(hdr.src_path());
    if (src) {
        encap.set_src_path(*src);
    } else {
        encap.set_src_path(hdr.dst_path());
    }
		
    // clear the paint
    SET_XIA_PAINT_ANNO(p, DESTINED_FOR_LOCALHOST);

    // send the packet out to the network
    output(0).push(encap.encap(p));

    delete[] msg;
    msg = NULL;

    return;
}


// processes a data packet that should be been sent somewhere else
void
XCMP::processBadForwarding(Packet *p_in) {
    XIAHeader hdr(p_in);


    const struct click_xia* shdr = p_in->xia_header();
    int last = shdr->last;
    if(last < 0)
        last += shdr->dnode;
    const struct click_xia_xid_node& lastnode = shdr->node[last];
    XID lnode(lastnode.xid);

    if(DEBUG)
        click_chatter("%s: %s sent me a packet that should route on its local network (Dst: %s)\n", 
                      _src_path.unparse().c_str(), hdr.src_path().unparse().c_str(), 
                      XID(lastnode.xid).unparse().c_str());

    // get the next hop information stored in the packet annotation
    XID nxt_hop = p_in->nexthop_neighbor_xid_anno();

    sendXCMPPacket(p_in, XCMP_REDIRECT, XCMP_REDIRECT_HOST, &lnode.xid(), &nxt_hop.xid(), &_src_path); // send a redirect
	   
    if(DEBUG)
        click_chatter("%s: Redirect sent\n", _src_path.unparse().c_str());

    return;
}

// processes a data packet that was undeliverable
void
XCMP::processUnreachable(Packet *p_in) {
    XIAHeader hdr(p_in);
    if(hdr.nxt() == CLICK_XIA_NXT_XCMP && ((unsigned char *)hdr.payload())[0] == XCMP_UNREACH) { // can't deliever an unreachable?
        // return immediately to avoid infinite packet generation
        return;
    }


    String broadcast_xid(BHID);  // broadcast HID
    XID bcast_xid;
    bcast_xid.parse(broadcast_xid);    

    XIAPath dst_path = hdr.dst_path();
    if (!dst_path.is_valid()) {
        click_chatter("xcmp: discarding invalid path. %s\n", dst_path.unparse_re().c_str() );
	        click_chatter("%s: Dest (S: %s , D: %s) Unreachable\n", _src_path.unparse().c_str(), 
                      hdr.src_path().unparse().c_str(), 
                      hdr.dst_path().unparse().c_str());
        return;
    }
    dst_path.remove_node(dst_path.destination_node());
    if(dst_path.unparse_node_size() < 1) {
        return;
    }
    XID dst_hid = dst_path.xid(dst_path.destination_node());
    if(dst_hid == bcast_xid) {
        // don't send undeliverables back to broadcast packets
        return;
    }

    if(DEBUG)
        click_chatter("%s: Dest (S: %s , D: %s) Unreachable\n", _src_path.unparse().c_str(), 
                      hdr.src_path().unparse().c_str(), 
                      hdr.dst_path().unparse().c_str());

    sendXCMPPacket(p_in, XCMP_UNREACH, XCMP_UNREACH_HOST, NULL, NULL, &_src_path); // send an undeliverable message

    if(DEBUG)
        click_chatter("%s: Dest Unreachable sent to %s\n", _src_path.unparse().c_str(), 
                      hdr.src_path().unparse().c_str());

    return;
}

// process a data packet that had expired
void
XCMP::processExpired(Packet *p_in) {
    if(DEBUG)
        click_chatter("%s: HLIM Exceeded\n", _src_path.unparse().c_str());
    
    sendXCMPPacket(p_in, XCMP_TIMXCEED, XCMP_TIMXCEED_REASSEMBLY , NULL, NULL, &_src_path); // send a TTL expire message
  		
    if(DEBUG)
        click_chatter("%s: TIME EXCEEDED sent\n", _src_path.unparse().c_str());
		
    return;
}

// receive a ping and send a pong back to the sender
void
XCMP::gotPing(const Packet *p_in) {
    const XIAHeader hdr(p_in);
    if(DEBUG)
        click_chatter("%s: PING received; client seq = %u\n", _src_path.unparse().c_str(), 
                      *(uint16_t*)(hdr.payload() + 6));

    sendXCMPPacket(p_in, XCMP_ECHOREPLY, 0, NULL, NULL, NULL);

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
        click_chatter("%s: PONG sent; client seq = %u\n", _src_path.unparse().c_str(), 
                      *(uint16_t*)(p_in->data() + 6));
}

// got pong, send up
void
XCMP::gotPong(Packet *p_in) {
    XIAHeader hdr(p_in);
    if(DEBUG)
        click_chatter("%s: PONG recieved; client seq = %u\n", _src_path.unparse().c_str(), *(uint16_t*)(hdr.payload() + 6));
    sendUp(p_in);
}

// got expiry packet, send up
void
XCMP::gotExpired(Packet *p_in) {
    if(DEBUG)
        click_chatter("%s: Received TIME EXCEEDED\n", _src_path.unparse().c_str());
    sendUp(p_in);
}

// got unreachable packet, send up
void
XCMP::gotUnreachable(Packet *p_in) {
    if(DEBUG)
        click_chatter("%s: Received UNREACHABLE\n", _src_path.unparse().c_str());
    sendUp(p_in);		
}

// got redirect packet, send up
void
XCMP::gotRedirect(Packet *p_in) {
    if(DEBUG)
        click_chatter("%s: Received REDIRECT\n", _src_path.unparse().c_str());
    
    XIAHeader hdr(p_in);
    const uint8_t *pay = hdr.payload();
    const size_t xs = sizeof(struct click_xia_xcmp);
    XID baddest, newroute;
    XIAHeader *badhdr;

    baddest = XID((const struct click_xia_xid &)(pay[xs]));
    newroute = XID((const struct click_xia_xid &)(pay[xs+sizeof(struct click_xia_xid)]));
    badhdr = new XIAHeader((const struct click_xia *)(&pay[xs+sizeof(struct click_xia_xid)*2]));
    if(DEBUG)
        click_chatter("%s: REDIRECT INFO: %s told me (%s) that in order to send to %s, I should first send to %s\n",
                      _src_path.unparse().c_str(), hdr.src_path().unparse().c_str(), 
                      hdr.dst_path().unparse().c_str(), baddest.unparse().c_str(), newroute.unparse().c_str());
		
    delete badhdr;

    // limit the max size for safety
    const size_t msize = 1024; 
    char msg[msize];

    // truncate
    size_t cplen = hdr.plen();
    if (hdr.plen()>msize) {
        cplen = msize;
        if(DEBUG)
            click_chatter("Truncating XCMP_REDIRECT because the size (hdr.plen()) %d is > max (%d)", hdr.plen(), msize);
    }

    memcpy(msg, hdr.payload(), cplen);
				
    // create a packet to store the message in
    WritablePacket *p = Packet::make(256, msg, cplen, 0);

    XIAPath dest;
    XIAPath::handle_t d_dummy = dest.add_node(XID());
    XIAPath::handle_t d_node = dest.add_node(XID((const struct click_xia_xid &)(pay[xs])));
    dest.add_edge(d_dummy, d_node);

    // encapsulate the packet within XCMP
    XIAHeaderEncap encap;
    encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
    encap.set_dst_path(dest);
    encap.set_src_path(_src_path);

    // paint the packet so the upper level routing process
    // knows to update its table
    SET_XIA_PAINT_ANNO(p, 1);
		
    // send the Route Update to Host
    output(1).push(encap.encap(p));

    p_in->kill();
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

    case XCMP_ECHO: // PING
        gotPing(p_in);
        break;
	  
    case XCMP_ECHOREPLY: // PONG
        gotPong(p_in);
        break;

    case XCMP_TIMXCEED: // Time EXCEEDED (ie. TTL expiration)
        gotExpired(p_in);
		break;

    case XCMP_UNREACH: // XID unreachable
        gotUnreachable(p_in);
		break;
		
    case XCMP_REDIRECT: // redirect
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
