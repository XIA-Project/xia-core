/*
 * xcmp.(cc,hh) -- element that handles sending and receiving xcmp messages
 */

#include <click/config.h>
#include "xcmp.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
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

	// first check to see if this packet hop-limit has expired
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
      WritablePacket *p = Packet::make(256, msg, hdr.plen(), 0);
      
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

		// send pong up to local host
		output(1).push(p_in);
        break;

    case 11: // Time EXCEEDED (ie. TTL expiration)
	    if(DEBUG)
		    click_chatter("%s: %u: Received TIME EXCEEDED\n", _src_path.unparse().c_str(), Timestamp::now().usecval());

		// send Time EXCEEDED up to local host
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
