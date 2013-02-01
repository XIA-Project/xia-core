/*
 * xiafilterpackets.{cc,hh} -- 
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
#include "xiafilterpackets.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

XIAFilterPackets::XIAFilterPackets()
{
}

XIAFilterPackets::~XIAFilterPackets()
{
}

int
XIAFilterPackets::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (cp_va_kparse(conf, this, errh,
		     "PORT", cpkP + cpkM, cpInteger, &_port,
		     cpEnd) < 0)
      return -1;


    return 0;
}

void
XIAFilterPackets::push(int port, Packet *p_in)
{
  const click_udp * uh = p_in->udp_header();
  unsigned short _dport = ntohs(uh->uh_dport);
  if(_dport == _port)
    output(0).push(p_in);
  else if (_dport == _port+1)
    output(1).push(p_in);
  else
    p_in->kill();
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAFilterPackets)
ELEMENT_MT_SAFE(XIAFilterPackets)
