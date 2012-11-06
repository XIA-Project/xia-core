/*
 * xiapaintswitch.{cc,hh} -- element routes packets to one output of several
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
#include "xiapaintswitch.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

XIAPaintSwitch::XIAPaintSwitch()
{
}

XIAPaintSwitch::~XIAPaintSwitch()
{
}

int
XIAPaintSwitch::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int anno = XIA_PAINT_ANNO_OFFSET;
    if (Args(conf, this, errh).read_p("ANNO", AnnoArg(1), anno).complete() < 0)
	  return -1;
    _anno = anno;
    return 0;
}

void
XIAPaintSwitch::push(int, Packet *p)
{
    int output_port = static_cast<int>(p->anno_u8(_anno));
    if (output_port != 0xFF)
	checked_output_push(output_port, p);
    else { // duplicate to all output ports
	int n = noutputs();
	for (int i = 0; i < n - 1; i++)
	    if (Packet *q = p->clone())
		output(i).push(q);
	output(n - 1).push(p);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAPaintSwitch)
ELEMENT_MT_SAFE(XIAPaintSwitch)
