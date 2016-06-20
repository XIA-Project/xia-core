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
#ifndef CLICK_XIACHALLENGESOURCE_HH
#define CLICK_XIACHALLENGESOURCE_HH
#include <click/element.hh>
#include <click/glue.hh>
#include <click/atomic.hh>
#include <clicknet/xia.h>
#include <click/xiapath.hh>
#include <click/hashtable.hh>
#include <string>
CLICK_DECLS

/*
  =c

  XIAChallengeSource(SRC, DST [, PRINT_EVERY])

  =s xia


  =d


  =e


*/

class XIAChallengeSource : public Element { public:

	XIAChallengeSource();
    ~XIAChallengeSource();

    const char *class_name() const		{ return "XIAChallengeSource"; }
    const char *port_count() const		{ return "1/2"; }
    const char *processing() const		{ return PUSH; }

    int configure(Vector<String> &, ErrorHandler *);
    int initialize(ErrorHandler *);

    void push(int, Packet *);

private:
	bool generate_secret();
    void send_challenge(Packet *);
	void verify_response(Packet *);
	bool is_verified(Packet *);
	String src_hid_str(Packet *);
	int digest_to_hex_string(unsigned char *, int, char *, int );
    int _active;
	int _iface;
	char* _name;
	const static size_t router_secret_length = 10;
	char router_secret[router_secret_length];
	String pub_path;
	String priv_path;
	XIAPath _src_path;
    HashTable<String, short> _verified_table;
};

CLICK_ENDDECLS
#endif
