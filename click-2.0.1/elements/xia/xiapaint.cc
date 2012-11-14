/*
 * xiapaint.{cc,hh} -- element sets packets' xiapaint annotation
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
#include "xiapaint.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

XIAPaint::XIAPaint()
{
}

XIAPaint::~XIAPaint()
{
}

int
XIAPaint::configure(Vector<String> &conf, ErrorHandler *errh)
{
    int anno = XIA_PAINT_ANNO_OFFSET;
    if (Args(conf, this, errh)
	.read_mp("COLOR", _color)
	.read_p("ANNO", AnnoArg(1), anno).complete() < 0)
	return -1;
    _anno = anno;
    return 0;
}

Packet *
XIAPaint::simple_action(Packet *p)
{
    p->set_anno_s16(_anno, _color);
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAPaint)
ELEMENT_MT_SAFE(XIAPaint)
