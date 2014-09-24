/*
 * xiachallengeresponder.(cc,hh) --
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
#include "xiachallengeresponder.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xid.hh>
#include <click/xiaheader.hh>
#include <click/xiasecurity.hh>
#include <click/packet_anno.hh>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

CLICK_DECLS

#define CLICK_XIA_NXT_XCHAL		65	/*  Challenge */
#define CLICK_XIA_NXT_XRESP		66	/*  Response */

#define MAX_SIGNATURE_SIZE 256

// NOTE:
// Challenge = ChallengeComponents, HMAC
// ChallengeComponents = srcHID, dstHID, interface, originalPacketHash
// Response = Challenge, Signature, ResponderPubkey
//

// no initialization needed
XIAChallengeResponder::XIAChallengeResponder() //: _timer(this)
{

}

// no cleanup needed
XIAChallengeResponder::~XIAChallengeResponder()
{

}

/*void
XIAChallengeResponder::run_timer(Timer *timer) // TODO: who calls this?
{
    _ttl-= SHUTOFF_RESET;
    if(_ttl < 0) {
	_shutoff = 0;
	_ttl = 0;
    }

    //click_chatter("RAN TIMER!");

    _hashtable.clear();
    _timer.schedule_after_sec(SHUTOFF_RESET);
}*/


int
XIAChallengeResponder::configure(Vector<String> &conf, ErrorHandler *errh)
{
	XID local_hid;

    if (cp_va_kparse(conf, this, errh,
			 "LOCALHID", cpkP + cpkM, cpXID, &local_hid,
		     "ACTIVE", 0, cpInteger, &_active,
		     cpEnd) < 0)
        return -1;
  /*  _shutoff = false;
    _ttl = 0;
    _timer.initialize(this);
    _timer.schedule_after_sec(SHUTOFF_RESET);*/

	local_hid_str = local_hid.unparse().c_str();

	outgoing_header = 0;
    return 0;
}

int
XIAChallengeResponder::initialize(ErrorHandler *)
{
    return 0;
}


void
XIAChallengeResponder::processChallenge(Packet *p_in)
{
	int i;
	char* pch;
	uint16_t length;
	char hash_str[XIA_SHA_DIGEST_STR_LEN];

	// Get the src and dst addresses
	XIAHeader xiah(p_in->xia_header());
	XIAPath src_dag = xiah.src_path();
	XIAPath dst_dag = xiah.dst_path();

	// Retrieve challenge from payload
	XIASecurityBuffer challenge((const char *)xiah.payload(), xiah.plen());

	// challengeComponents = src_hid, dst_hid, interface, packetHash
	uint16_t challengeEntriesLength = challenge.peekUnpackLength();
	char challengeEntries[challengeEntriesLength];
	challenge.unpack((char *)challengeEntries, &challengeEntriesLength);
	XIASecurityBuffer challengeComponents(challengeEntries, challengeEntriesLength);

	// hmac
	length = challenge.peekUnpackLength();
	char hmac[length];
	challenge.unpack((char *)hmac, &length);

	// Retrieve the entries in challengeComponents
	length = challengeComponents.peekUnpackLength();
	char challenge_src_hid[length + 1];  // null terminate
	bzero(challenge_src_hid, length + 1);
	challengeComponents.unpack((char *)challenge_src_hid, &length);

	length = challengeComponents.peekUnpackLength();
	char challenge_dst_hid[length + 1];  // null terminate
	bzero(challenge_dst_hid, length + 1);
	challengeComponents.unpack((char *)challenge_dst_hid, &length);

	length = challengeComponents.peekUnpackLength();
	int interface;
	challengeComponents.unpack((char *)&interface, &length);

	length = challengeComponents.peekUnpackLength();
	uint8_t packetHash[length];
	challengeComponents.unpack((char *)packetHash, &length);

	xs_hexDigest(packetHash, SHA_DIGEST_LENGTH, hash_str, XIA_SHA_DIGEST_STR_LEN);

	click_chatter("== Challenge Packet detail ============================================================");
	click_chatter("\tSRC: %s", challenge_src_hid);
	click_chatter("\tDST: %s", challenge_dst_hid);
	click_chatter("\tiface: %d", interface);
	click_chatter("\thash: %s", hash_str);
	click_chatter("=======================================================================================");

	click_chatter("H> Verifying challenge is in response to a sent packet");
	if(!check_outgoing_hashes(packetHash)) {
		click_chatter("H> FAILED: challenge doesn't match a sent packet");
		return;
	}

	click_chatter("H> Sending response...");

	// Build the response payload
	// challenge, signature, pubkey

	// Challenge
	XIASecurityBuffer response(512);
	response.pack((char *)challenge.get_buffer(), challenge.size());

	// Signature
	char signature[MAX_SIGNATURE_SIZE];
	uint16_t signatureLength = MAX_SIGNATURE_SIZE;
	xs_sign(local_hid_str.c_str(), (unsigned char *)challenge.get_buffer(), challenge.size(), (unsigned char *)signature, &signatureLength);
	response.pack((char *)signature, signatureLength);

	/*
	char hex_signature[257];
	digest_to_hex_string(signature, signatureLength, hex_signature, 257);
	click_chatter("Response Signature: %s", hex_signature);
	*/

	// Pubkey
	char pubkey[MAX_PUBKEY_SIZE];
	uint16_t pubkeyLength = MAX_PUBKEY_SIZE;
	if(xs_getPubkey(local_hid_str.c_str(), pubkey, &pubkeyLength)) {
		click_chatter("XIAChallengeResponder::processChallenge ERROR local public key not found");
		return;
	}
	response.pack((char *)pubkey, pubkeyLength);

	// Make the challenge packet header
	WritablePacket *response_p = Packet::make(256, response.get_buffer(), response.size(), 0);
	XIAHeaderEncap encap;
	encap.set_nxt(CLICK_XIA_NXT_XRESP);
	encap.set_dst_path(src_dag);
	encap.set_src_path(dst_dag); // TODO: do something smarter here (e.g., don't include original dest SID)

	output(1).push(encap.encap(response_p));
}


int
XIAChallengeResponder::digest_to_hex_string(unsigned char *digest, int digest_len, char *hex_string, int hex_string_len)
{
    int i;
    int retval = -1;
    if(hex_string_len != (2*digest_len) + 1) {
        return retval;
    }
    for(i=0;i<digest_len;i++) {
        sprintf(&hex_string[2*i], "%02x", (unsigned int)digest[i]);
    }
    hex_string[hex_string_len-1] = '\0';
    retval = 0;
    return retval;
}

bool
XIAChallengeResponder::check_outgoing_hashes(uint8_t *digest)
{
	bool result = false;
	// Walk through hashes of sent packets until a match is found
	for(int i=0;i<num_outgoing_hashes;i++) {
		if(memcmp(digest, outgoing_hashes[i], hash_length) == 0) {
			click_chatter("H> Found matching header at offset %d", i);
			result = true;
			break;
		}
	}
	return result;
}

void
XIAChallengeResponder::store_outgoing_hash(Packet *p_in)
{
	xs_getSHA1Hash(p_in->data(), p_in->length(), outgoing_hashes[outgoing_header], SHA_DIGEST_LENGTH);
	outgoing_header = (outgoing_header + 1) % num_outgoing_hashes;
}

void
XIAChallengeResponder::push(int in_port, Packet *p_in)
{
	// Packets going to the network
	if(in_port == 1) {
		if(_active) {
			// Store hash in recent packets buffer
			store_outgoing_hash(p_in);
		}
		// Forward packet out on port 2 to be discarded
		output(2).push(p_in);
		return;
	}
    if(_active) {
		if(in_port == 0) { // Packet from network
			// Is the packet a challenge packet?
			if(p_in->xia_header()->nxt == CLICK_XIA_NXT_XCHAL) {
				click_chatter("H> Challenge received");
				processChallenge(p_in);
				p_in->kill();
				return;
			} else { // regular packet from router
				//click_chatter("Just a regular, %d", p_in->xia_header()->nxt);
				output(in_port).push(p_in);
				return;
			}
		}
    } else { // not active
		output(in_port).push(p_in);
    }
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAChallengeResponder)
ELEMENT_MT_SAFE(XIAChallengeResponder)
