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
#include <click/xiaheader.hh>
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
	uint8_t pub_key[248];
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
	String key_path;

    if (cp_va_kparse(conf, this, errh,
	 		 "KEYPATH", cpkP + cpkM, cpString, &key_path,
		     "ACTIVE", 0, cpInteger, &_active,
		     cpEnd) < 0)
        return -1;
  /*  _shutoff = false;
    _ttl = 0;
    _timer.initialize(this);
    _timer.schedule_after_sec(SHUTOFF_RESET);*/

	pub_path.append(key_path.c_str(), strlen(key_path.c_str()));
	pub_path.append("pub", 3);
	priv_path.append(key_path.c_str(), strlen(key_path.c_str()));
	priv_path.append("priv", 4);

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

	// Get the src and dst addresses
	XIAHeader xiah(p_in->xia_header());
	XIAPath src_dag = xiah.src_path();
	XIAPath dst_dag = xiah.dst_path();
    click_xia_challenge *challenge = (click_xia_challenge *)xiah.payload();

	click_chatter("== Challenge Packet detail ============================================================");
	click_chatter("\tSRC: %s", challenge->body.src_hid);
	click_chatter("\tDST: %s", challenge->body.dst_hid);
	click_chatter("\tiface: %d", challenge->body.iface);
	click_chatter("\thash 1st byte: %0X", challenge->body.hash[0]);
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



void
XIAChallengeResponder::sign(uint8_t *sig, struct click_xia_challenge *challenge)
{
	// Hash
    SHA1_ctx sha_ctx;
    unsigned char digest[20];
    SHA1_init(&sha_ctx);
    SHA1_update(&sha_ctx, (unsigned char *)challenge, sizeof(click_xia_challenge));
    SHA1_final(digest, &sha_ctx);

	//click_chatter("hash: %X", digest[0]);

	// Encrypt with private key
	FILE *fp = fopen(priv_path.c_str(), "r");
	RSA *rsa = PEM_read_RSAPrivateKey(fp,NULL,NULL,NULL);
	if(rsa==NULL)
		ERR_print_errors_fp(stderr);

 	unsigned char *sig_buf = NULL;
    unsigned int sig_len = 0;
    sig_buf = (unsigned char*)malloc(RSA_size(rsa));
    if (NULL == sig) { click_chatter("sig malloc failed"); }

    int rc = RSA_sign(NID_sha1, digest, sizeof digest, sig_buf, &sig_len, rsa);
    if (1 != rc) { click_chatter("RSA_sign failed"); }

	//click_chatter("Sig: %X Len: %d", sig_buf[0], sig_len);
	memcpy(sig, sig_buf, sig_len);

	fclose(fp);

}

void
XIAChallengeResponder::get_pubkey(uint8_t *pub)
{
	FILE *fp = fopen(pub_path.c_str(), "r");
	RSA *rsa = PEM_read_RSAPublicKey(fp,NULL,NULL,NULL);
	if(rsa==NULL)
		ERR_print_errors_fp(stderr);

	int keylen_pub;
	char *pem_pub;

	BIO *bio_pub = BIO_new(BIO_s_mem());
	PEM_write_bio_RSAPublicKey(bio_pub, rsa);
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

void
XIAChallengeResponder::hash(uint8_t *dig, Packet *p_in)
{
    const click_xia *h = p_in->xia_header();
    int plen = h->plen;
    int hsize = 8 + (h->dnode+h->snode)*sizeof(click_xia_xid_node);
    SHA1_ctx sha_ctx;
    unsigned char digest[20];
    SHA1_init(&sha_ctx);
    SHA1_update(&sha_ctx, (unsigned char *)h, plen+hsize);
    SHA1_final(digest, &sha_ctx);

    memcpy(dig, digest, 20);
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
	hash(outgoing_hashes[outgoing_header], p_in);
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
