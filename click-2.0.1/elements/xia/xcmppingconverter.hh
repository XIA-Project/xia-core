/*
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
#ifndef CLICK_XCMPPINGCONVERTER_HH
#define CLICK_XCMPPINGCONVERTER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
#include <click/timer.hh>
CLICK_DECLS

/*
=c

XCMPPingConverter(SRC [, PRINT_EVERY])

=s xia

replaces incoming packets with XCMP ping requests from SRC to DST IP converted to HID (HIP encoding)

=d

Sends XCMP ping packets with SRC as source when 
receiving a packet on input. Uses DST IP from packet
as DST HID (HIP encoding). Printing out a message for 
every PRINT_EVERY pings sent.

=e

Send pings from AD1 HID1, printing every other packet sent.
XCMPPingConverter(RE AD1 HID1, 2)

*/

class XCMPPingConverter : public Element { public:

    XCMPPingConverter();
    ~XCMPPingConverter();
  
    const char *class_name() const		{ return "XCMPPingConverter"; }
    const char *port_count() const		{ return "0-1/1"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

  void makePacket(XIAPath *);
  void run_timer(Timer *);

    void push(int, Packet *);

  private:

  Timer _timer;
  int _interval;
    // for the ping packet
    XIAPath _src_path;
  XIAPath _dst_path;
    uint16_t _seqnum;

    // how often to print message to console
    uint32_t _print_every;
};

CLICK_ENDDECLS
#endif
