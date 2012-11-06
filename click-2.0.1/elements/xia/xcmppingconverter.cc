/*
 * xcmppingconverter.(cc,hh) -- element that sends xcmp pings upon receiving a packet
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
#include "xcmppingconverter.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>
CLICK_DECLS

// initialize the sequence number
XCMPPingConverter::XCMPPingConverter() : _timer(this)
{
    _seqnum = 0;
}

// no cleanup needed
XCMPPingConverter::~XCMPPingConverter()
{

}

// allow users to modify SRC and the frequency of prints
int
XCMPPingConverter::configure(Vector<String> &conf, ErrorHandler *errh)
{
    _print_every = 1;

    if (cp_va_kparse(conf, this, errh,
                   "SRC", cpkP+cpkM, cpXIAPath, &_src_path,
                   "PRINT_EVERY", 0, cpInteger, &_print_every,
					 "INT", 0, cpInteger, &_interval,
					 //"DST", cpkP+cpkM, cpXIAPath, &_dst_path,
                   cpEnd) < 0)
        return -1; // error config

    return 0;
}

int
XCMPPingConverter::initialize(ErrorHandler *)
{
  _timer.initialize(this);
  //_timer.schedule_after(Timestamp(_interval));
    return 0;
}

void
XCMPPingConverter::makePacket(XIAPath *dst)
{
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
  encap.set_dst_path(*dst);
  encap.set_src_path(_src_path);
  
  // print out the packet if requested during setup
  if (_seqnum % _print_every == 0)
	click_chatter("%s: %u: converted packet into PING (%u) (Dst: %s)\n", _src_path.unparse().c_str(),Timestamp::now().usecval(),_seqnum,dst->unparse().c_str());
  
  _seqnum++;

  //p->set_anno_u32(Packet::dst_ip_anno_offset, p_in->ip_header()->ip_dst.s_addr);
  
  //return encap.encap(p);
  output(0).push(encap.encap(p));
}

void
XCMPPingConverter::run_timer(Timer *)
{
  makePacket(&_dst_path);
  _timer.reschedule_after(Timestamp(_interval));
}

// read packets in from the network, convert it into a ping packet
void
XCMPPingConverter::push(int, Packet *p_in)
{
	const click_ip *ip = p_in->ip_header();
	IPAddress dst(ip->ip_dst);
	String dstAddr = dst.unparse();
	String hipDst("RE ADIP:"+dstAddr +" HIP:"+dstAddr);
	XIAPath xp;
	xp.parse(hipDst, NULL);

  makePacket(&xp);
  p_in->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XCMPPingConverter)
ELEMENT_MT_SAFE(XCMPPingConverter)
