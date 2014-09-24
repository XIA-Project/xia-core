/* -*- c-basic-offset: 4 -*-
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

#ifndef CLICK_XIACHALLENGERESPONDER_HH
#define CLICK_XIACHALLENGERESPONDER_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/hashtable.hh>
#include <click/xiapath.hh>
#include <click/timer.hh>
#include <elements/ipsec/sha1_impl.hh>

CLICK_DECLS

/*
=c

XIAChallengeResponder

=s xia

=d

=e


*/

//#define SHUTOFF_RESET 5

class XIAChallengeResponder : public Element { public:

    XIAChallengeResponder();
    ~XIAChallengeResponder();

    const char *class_name() const		{ return "XIAChallengeResponder"; }
    const char *port_count() const		{ return "2/3"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    void push(int, Packet *);

  private:
//	std::string hash(Packet *);
	void get_pubkey(uint8_t *);
	void sign(uint8_t *, struct click_xia_challenge *);
	void processChallenge(Packet *);
	bool check_outgoing_hashes(uint8_t *);
	void hash(uint8_t *, Packet *);
	void store_outgoing_hash(Packet *);
	int digest_to_hex_string(unsigned char *, int, char *, int );
	//void run_timer(Timer *);


    HashTable<String, short> _hashtable;
	const static int num_outgoing_hashes = 100;
	const static int hash_length = SHA_DIGEST_LENGTH;
	uint8_t outgoing_hashes[num_outgoing_hashes][hash_length];
	int outgoing_header;
	String local_hid_str;
   // int _shutoff;
    bool _active;
    //int _ttl;
    //Timer _timer;
};

CLICK_ENDDECLS
#endif
