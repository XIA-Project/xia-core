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
#ifndef CLICK_XNETJ_HH
#define CLICK_XNETJ_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
#include <click/etheraddress.hh>
#include <click/hashtable.hh>
#include <string>
CLICK_DECLS

/*
  =c

  XNetJ(SRC, DST [, PRINT_EVERY])

  =s xia


  =d


  =e


*/

class XNetJ : public Element { public:

	XNetJ();
    ~XNetJ();

    const char *class_name() const		{ return "XNetJ"; }
    const char *port_count() const		{ return "2/2"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    void push(int, Packet *);

private:
	char* _message;
	EtherAddress _my_en;
};

CLICK_ENDDECLS
#endif
