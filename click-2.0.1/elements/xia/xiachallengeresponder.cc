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

#define DEBUG 0

#define CLICK_XIA_NXT_XCHAL		65	/*  Challenge */
#define CLICK_XIA_NXT_XRESP		66	/*  Response */


struct click_xia_challenge_body{
		// TODO: change to src/dest full DAG
		uint8_t src_hid[48];
		uint8_t dst_hid[48];
		uint8_t hash[20];
		uint16_t	iface;
};


// XIA challenge query header
struct click_xia_challenge {
	struct click_xia_challenge_body body;
    uint8_t hmac[20];	// HMAC-SHA1(body, secret)
};

// XIA challenge reply header
struct click_xia_response {
    struct click_xia_challenge v;
	uint8_t pub_key[274];
	uint8_t signature[128];
};

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
	String local_hid_str;

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
	pub_path = "key/" + local_hid_str.substring(4) + ".pub";
	priv_path = "key/" + local_hid_str.substring(4);

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
	char hash_str[XIA_SHA_DIGEST_STR_LEN];

	// Get the src and dst addresses
	XIAHeader xiah(p_in->xia_header());
	XIAPath src_dag = xiah.src_path();
	XIAPath dst_dag = xiah.dst_path();
    click_xia_challenge *challenge = (click_xia_challenge *)xiah.payload();
	xs_hexDigest(challenge->body.hash, 20, hash_str, XIA_SHA_DIGEST_STR_LEN);

	click_chatter("== Challenge Packet detail ============================================================");
	click_chatter("\tSRC: %s", (char *)challenge->body.src_hid);
	click_chatter("\tDST: %s", (char *)challenge->body.dst_hid);
	click_chatter("\tiface: %d", challenge->body.iface);
	click_chatter("\thash: %s", hash_str);
	click_chatter("=======================================================================================");

	click_chatter("H> Verifying challenge is in response to a sent packet");
	if(!check_outgoing_hashes(challenge->body.hash)) {
		click_chatter("H> FAILED: challenge doesn't match a sent packet");
		return;
	}

	click_chatter("H> Sending response...");

	// Make the response packet payload
	struct click_xia_response response;

	// Challenge packet copy
	memcpy(&(response.v), challenge, sizeof(click_xia_challenge));

	// Sign challenge struct
	sign(response.signature, challenge);

	char hex_signature[257];
	digest_to_hex_string(response.signature, 128, hex_signature, 257);
	click_chatter("Response Signature: %s", hex_signature);

	// Self own public key
	get_pubkey(response.pub_key);

	// Make the challenge packet header
	WritablePacket *response_p = Packet::make(256, &response, sizeof(struct click_xia_response), 0);
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


void
XIAChallengeResponder::sign(uint8_t *sig, struct click_xia_challenge *challenge)
{
	// Hash
    SHA1_ctx sha_ctx;
    unsigned char digest[20];
    SHA1_init(&sha_ctx);
    SHA1_update(&sha_ctx, (unsigned char *)challenge, sizeof(click_xia_challenge));
    SHA1_final(digest, &sha_ctx);

	char hex_digest[41];
	if(digest_to_hex_string(digest, 20, hex_digest, 41)) {
		click_chatter("ERROR: Failed converting challenge hash to hex string");
		return;
	}
	click_chatter("Responder: Hash of challenge: %s", hex_digest);
	//click_chatter("hash: %X", digest[0]);

	// Encrypt with private key
	click_chatter("H> Signing with private key from: %s", priv_path.c_str());
	FILE *fp = fopen(priv_path.c_str(), "r");
	RSA *rsa = PEM_read_RSAPrivateKey(fp,NULL,NULL,NULL);
	if(rsa==NULL)
		ERR_print_errors_fp(stderr);

 	unsigned char *sig_buf = NULL;
    unsigned int sig_len = 0;
    sig_buf = (unsigned char*)malloc(RSA_size(rsa));
    if (NULL == sig) { click_chatter("sig malloc failed"); }

    //int rc = RSA_sign(NID_sha1, digest, sizeof digest, sig_buf, &sig_len, rsa);
    int rc = RSA_sign(NID_sha1, digest, 20, sig_buf, &sig_len, rsa);
    if (1 != rc) { click_chatter("RSA_sign failed"); }
	click_chatter("Responder signature length: %d", sig_len);

	//click_chatter("Sig: %X Len: %d", sig_buf[0], sig_len);
	memcpy(sig, sig_buf, sig_len);

	fclose(fp);

}

void
XIAChallengeResponder::get_pubkey(uint8_t *pub)
{
	click_chatter("H> Getting public key from: %s", pub_path.c_str());
	FILE *fp = fopen(pub_path.c_str(), "r");
	RSA *rsa = PEM_read_RSA_PUBKEY(fp,NULL,NULL,NULL);
	//RSA *rsa = PEM_read_RSAPublicKey(fp,NULL,NULL,NULL);
	if(rsa==NULL)
		ERR_print_errors_fp(stderr);

	int keylen_pub;
	char *pem_pub;

	BIO *bio_pub = BIO_new(BIO_s_mem());
	PEM_write_bio_RSA_PUBKEY(bio_pub, rsa);
	keylen_pub = BIO_pending(bio_pub);
	pem_pub = (char*)calloc(keylen_pub+1, 1); // Null-terminate
	BIO_read(bio_pub, pem_pub, keylen_pub);
	BIO_free_all(bio_pub);
//	click_chatter("Own Pub: %s", pem_pub);
//	click_chatter("keylen: %d", keylen_pub);
	RSA_free(rsa);
	fclose(fp);

	memcpy(pub, pem_pub, keylen_pub+1);

	free(pem_pub);
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
