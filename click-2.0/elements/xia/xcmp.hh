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

#ifndef CLICK_XCMP_HH
#define CLICK_XCMP_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
CLICK_DECLS

/*
=c

XCMP(SRC)

=s xia

Responds to various XCMP requests

=d

Responds to various XCMP requests. Output 0 sends packets out to the network.
Output 1 sends packets up to the local host

=e

An XCMP responder for host AD0 HID0
XCMP(RE AD0 HID0)

*/

// used for XCMP XID Unreachable
#define XARP_TIMEOUT 65

// used for XCMP Redirect
#define REDIRECT_BASE 70

class XCMP : public Element { public:

    XCMP();
    ~XCMP();

    const char *class_name() const		{ return "XCMP"; }
    const char *port_count() const		{ return "1/2"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    void push(int, Packet *);

  private:
    // ICMP-style checksum
    u_short in_cksum(u_short *, int);
    
    // source XIAPath of the local host
    XIAPath _src_path;
};

CLICK_ENDDECLS
#endif
