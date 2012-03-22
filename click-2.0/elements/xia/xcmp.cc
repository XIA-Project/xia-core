#include <click/config.h>
#include "xcmp.hh"
//#include "xiapingupdate.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

XCMP::XCMP()
{
  //_count = 0;
  //_connected = false;
}

XCMP::~XCMP()
{
}

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

void
XCMP::push(int, Packet *p_in)
{
    XIAHeader hdr(p_in);
    const uint8_t *payload = hdr.payload();

    if(hdr.hlim() <= 1) { // need to send a TIME EXCEEDED MESSAGE
      click_chatter("%s: %u: HLIM Exceeded\n", _src_path.unparse().c_str(),p_in->timestamp_anno().usecval());
      
      char msg[256];
      msg[0] = 11; // TIME EXCEEDED (type)
      msg[1] = 0; // TTL exceeded in transit (code)
      
      memcpy(&msg[8], hdr.hdr(), hdr.hdr_size()); // copy XIP header
      memcpy(&msg[8+hdr.hdr_size()], hdr.payload(), 8); // copy first 8 bytes of datagram
      
      uint16_t checksum = in_cksum((u_short *)msg, hdr.hdr_size()+16);
      memcpy(&msg[2], &checksum, 2);
      
      WritablePacket *p = Packet::make(256, msg, hdr.plen(), 0);
      
      XIAHeaderEncap encap;
      encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
      encap.set_dst_path(hdr.src_path());
      encap.set_src_path(_src_path);
      
      click_chatter("%s: %u: TIME EXCEEDED sent\n", _src_path.unparse().c_str(),Timestamp::now().usecval());
      
      output(0).push(encap.encap(p));
      p_in->kill();
      return;
    }

    if(hdr.nxt() != CLICK_XIA_NXT_XCMP) { // not XCMP
      p_in->kill();
      return;
    }

    switch (*payload) {
    case 8: // PING
        {
	  /*if (hdr.plen() != 8) {
                click_chatter("invalid PING message length\n");
                break;
		}*/

            click_chatter("%s: %u: PING received; client seq = %u\n", _src_path.unparse().c_str(),p_in->timestamp_anno().usecval(), *(uint16_t*)(payload + 6));

	    char pong[256];
	    memcpy(pong, hdr.payload(), hdr.plen());
	    //uint16_t checksumt2;
	    //memcpy(&checksumt2,&pong[2],2);

	    pong[2] = 0;
	    pong[3] = 0;

	    
	    //uint16_t checksumt = in_cksum((u_short *)pong, hdr.plen());
	    //printf("checksumt = %hu, checksumt2 = %hu\n",checksumt,checksumt2);


	    pong[0] = 0; // PONG

	    uint16_t checksum = in_cksum((u_short *)pong, hdr.plen());
	    //printf("assinging checksum (%hu)\n",checksum);
	    memcpy(&pong[2], &checksum, 2);

	    WritablePacket *p = Packet::make(256, pong, hdr.plen(), 0);
            
            XIAHeaderEncap encap;
            encap.set_nxt(CLICK_XIA_NXT_XCMP);   // XCMP
            encap.set_dst_path(hdr.src_path());
            encap.set_src_path(_src_path);

            //_connected = true;
            //_last_client = hdr.src_path();

            click_chatter("%s: %u: PONG sent; client seq = %u\n", _src_path.unparse().c_str(),Timestamp::now().usecval(), *(uint16_t*)(p->data() + 6));

            output(0).push(encap.encap(p));
            break;
        }

    case 0: // PONG
      click_chatter("%s: %u: PONG recieved; client seq = %u\n", _src_path.unparse().c_str(), Timestamp::now().usecval(), *(uint16_t*)(payload + 6));
      output(1).push(p_in);
      //click_chatter("ignoring PONG at XCMP\n");
        break;

	/*    case 102:
        // UPDATE
        {
            XIAPath new_path;
            new_path.parse_node(
                reinterpret_cast<const struct click_xia_xid_node*>(hdr.payload()),
                reinterpret_cast<const struct click_xia_xid_node*>(hdr.payload() + hdr.plen())
            );
            click_chatter("%u: updating XCMP with new address %s\n", p_in->timestamp_anno().usecval(), new_path.unparse(this).c_str());
            _src_path = new_path;

            if (_connected)
                output(0).push(XIAPingUpdate::make_packet(_src_path, _last_client, _src_path));


            break;
	    }*/

    case 11: // Time EXCEEDED (ie. TTL expiration)
      click_chatter("%s: %u: Received TIME EXCEEDED\n", _src_path.unparse().c_str(), Timestamp::now().usecval());
      output(1).push(p_in);
      break;

    default:
        click_chatter("invalid message type\n");
        break;
    }
    p_in->kill();
}

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
