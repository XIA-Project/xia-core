/*
** Copyright 2013 Carnegie Mellon University / ETH Zurich
** 
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
** 
** http://www.apache.org/licenses/LICENSE-2.0 
** 
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#ifndef SCIONDECAP_HH
#define SCIONDECAP_HH

#include <click/element.hh>
#include <click/hashcontainer.hh>
#include <click/hashallocator.hh>
#include <click/sync.hh>
#include <click/timer.hh>
#include <click/list.hh>
#include <clicknet/ip.h>
#include <list>
#include "scionpathinfo.hh"

CLICK_DECLS

class SCIONDecap : public Element { 

public:
    SCIONDecap();
    ~SCIONDecap();

    const char *class_name() const        { return "SCIONDecap"; }

    // input
    //   0: from SCION network (data plane & control plane)
    // output
    //   0: to SCION switch (data plane)
    
    const char *port_count() const        { return "1/1"; }
    const char *processing() const        { return AGNOSTIC; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    Packet *simple_action(Packet *);

private:
	SCIONPathInfo *_path_info;    
};

CLICK_ENDDECLS
#endif
