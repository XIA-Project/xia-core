/*
 * xiachallengesource.(cc,hh) --
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
#include "xiachallengesource.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xiaheader.hh>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>


CLICK_DECLS


#define CLICK_XIA_NXT_XCHAL		65	/*  Challenge */
#define CLICK_XIA_NXT_XRESP		66	/*  Response */


struct click_xia_challenge_body{
		// TODO: change to src/dest full DAG
		uint8_t src_hid[48];
		uint8_t dst_hid[48];
		uint8_t hash[20];
		uint16_t iface;
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


XIAChallengeSource::XIAChallengeSource()
{

}

// no cleanup needed
XIAChallengeSource::~XIAChallengeSource()
{

}

// allow users to modify SRC/DST and the frequency of prints
int
XIAChallengeSource::configure(Vector<String> &conf, ErrorHandler *errh)
{
	String key_path;

    if (cp_va_kparse(conf, this, errh,
 					 "KEYPATH", cpkP + cpkM, cpString, &key_path,
					 "INTERFACE", cpkP + cpkM, cpInteger, &_iface,
					 "SRC", cpkP+cpkM, cpXIAPath, &_src_path,
					 "ACTIVE", 0, cpInteger, &_active,
                   cpEnd) < 0)
        return -1; // error config

	generate_secret();
	pub_path.append(key_path.c_str(), strlen(key_path.c_str()));
	pub_path.append("pub", 3);
	priv_path.append(key_path.c_str(), strlen(key_path.c_str()));
	priv_path.append("priv", 4);

	if(!strcmp( _src_path.unparse().c_str(), "RE AD:1000000000000000000000000000000000000001 HID:4b1ed30c60b8f301ad818175c652bf1d5f61e24b"))
		_name = "R1";
	else if(!strcmp( _src_path.unparse().c_str(), "RE AD:1000000000000000000000000000000000000000 HID:f114f1187fbb008d54d8f99f51999fe20e1151db"))
		_name = "R0";
	else
		_name = "?";


    return 0;
}

int
XIAChallengeSource::initialize(ErrorHandler *)
{
    return 0;
}

void
XIAChallengeSource::generate_secret()
{
    static const char characters[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    for (int i=0;i<router_secret_length;i++) {
        router_secret[i] = characters[rand() % (sizeof(characters) - 1)];
    }
    router_secret[router_secret_length] = 0;
}

void
XIAChallengeSource::hash(uint8_t *dig, Packet *p_in)
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

	//return std::string((const char*)digest);
}

void
XIAChallengeSource::verify_response(Packet *p_in)
{
    int i;
	char* pch;
	char src_hid[48];

	// Get the src and dst addresses
	XIAHeader xiah(p_in->xia_header());
	XIAPath src_dag = xiah.src_path();
	XIAPath dst_dag = xiah.dst_path();
    click_xia_response *response = (click_xia_response *)xiah.payload();

	click_chatter("== Response Packet detail ============================================================");
	click_chatter("\tSRC: %s", response->v.body.src_hid);
	click_chatter("\tDST: %s", response->v.body.dst_hid);
	click_chatter("\tiface: %d", response->v.body.iface);
	click_chatter("\thash 1st byte: %0X", response->v.body.hash[0]);
	click_chatter("\tPub K: \n%s", response->pub_key);
	click_chatter("\tSig: %X", response->signature[0]);
	click_chatter("======================================================================================");


	// Verify HMAC: Check if the challenge message really originated from here
	unsigned int hmac_len = 20;
	uint8_t hmac[hmac_len];
	HMAC(router_secret, router_secret_length, (unsigned char *)&(response->v.body), sizeof(response->v.body), hmac, &hmac_len);
	if(memcmp(hmac, response->v.hmac, hmac_len)) {
		click_chatter("%s> Error: Invalid hmac returned with response", _name);
		return;
	}

	// Verify public key: Check if hash(pub) == src_hid
	char* pem_pub;
	int keylen_pub;
	std::string str((char*)response->pub_key);
	str.replace(str.find("-----BEGIN RSA PUBLIC KEY-----"), sizeof("-----BEGIN RSA PUBLIC KEY-----"), "");
	str.replace(str.find("-----END RSA PUBLIC KEY-----"), sizeof("-----END RSA PUBLIC KEY-----"), "");
	pem_pub = (char*)calloc(strlen(str.c_str())+1, 1);
	strcpy(pem_pub, str.c_str());
	keylen_pub = strlen(pem_pub);

	SHA1_ctx sha_ctx;
    unsigned char hash_pub[20];
    SHA1_init(&sha_ctx);
    SHA1_update(&sha_ctx, (unsigned char *)pem_pub, keylen_pub);
    SHA1_final(hash_pub, &sha_ctx);

	char *pos = (char*)response->v.body.src_hid + 4;	// Convert HID string to byte array
    unsigned char hid_byte[20];
    for(i = 0; i < 20; i++) {
        sscanf(pos, "%2hhx", &hid_byte[i]);
        pos += 2 * sizeof(char);
    }

	int pk_verified = 0;
	pk_verified = !memcmp(hash_pub, hid_byte, 20);
	if(!pk_verified)
	{
		click_chatter("%s> Error: Invalid public key", _name);
		return;
	}

	click_chatter("%s> Public key is authentic", _name);

	// Verify signature: Check if hash(v) == RSA_verify(sig, pub)
    unsigned char digest[20];
    SHA1_init(&sha_ctx);
    SHA1_update(&sha_ctx, (unsigned char *)&(response->v), sizeof(click_xia_challenge));
    SHA1_final(digest, &sha_ctx);

	BIO *bio_pub = BIO_new(BIO_s_mem());
	BIO_write(bio_pub, response->pub_key, 247);
	RSA *rsa = PEM_read_bio_RSAPublicKey(bio_pub, NULL, NULL, NULL);

	unsigned char *sig_buf = NULL;
    unsigned int sig_len = 128;
    sig_buf = (unsigned char*)malloc(RSA_size(rsa));
	memcpy(sig_buf, response->signature, sig_len);

	int sig_verified = RSA_verify(NID_sha1, digest, sizeof digest, sig_buf, sig_len, rsa);
	RSA_free(rsa);

	if (!sig_verified)
	{
		click_chatter("%s> Error: Invalid signature", _name);
		return;
	}

	click_chatter("%s> Signature is verified. Accept this HID", _name);

	String hid((char*)response->v.body.src_hid);
	_verified_table[hid] = 1;

}

// Check whether pack/et's source HID has been verified
bool
XIAChallengeSource::is_verified(Packet *p)
{
	char* pch;
	char src_hid[48];
    XIAHeader xiah(p->xia_header());
    XIAPath src_dag = xiah.src_path();
	pch = strstr ((char*)src_dag.unparse().c_str(), "HID:");
	strncpy (src_hid, pch, 44);
	src_hid[44] = '\0';
	String hid(src_hid);
	click_chatter("%s> Verifying %s", _name, src_hid);

	return _verified_table[hid] == 1;
}


// send a challenge packet to the source of the packet
void
XIAChallengeSource::send_challenge(Packet *p)
{
	int i;
	char* pch;

	// Get the src and dst addresses
	XIAHeader xiah(p->xia_header());
	XIAPath src_dag = xiah.src_path();
	XIAPath dst_dag = xiah.dst_path();

	// Make the challenge packet payload
	struct click_xia_challenge challenge;
	// Src HID
	pch = strstr ((char*)src_dag.unparse().c_str(), "HID:");
	strncpy ((char*)challenge.body.src_hid, pch, 4+40);
	challenge.body.src_hid[44] = '\0';
//    click_chatter("SRC: %s", challenge.body.src_hid);
//	click_chatter("Full SRC: %s", src_dag.unparse().c_str());
	// Dest HID

	char* pch2;
	pch2 = strstr ((char*)dst_dag.unparse().c_str(), "HID:");
	strncpy ((char*)challenge.body.dst_hid, pch2, 4+40);
	challenge.body.dst_hid[44] = '\0';
//    click_chatter("DST: %s", challenge.body.dst_hid);
//	click_chatter("DST[0]: %c %d", challenge.body.dst_hid[0], challenge.body.dst_hid[0]);
//	click_chatter("Full DST: %s", dst_dag.unparse().c_str());

	// Interface number
	challenge.body.iface = _iface;
//	click_chatter("iface: %d", _iface);

	hash(challenge.body.hash, p);
//	click_chatter("hash 1st byte: %0X", challenge.body.hash[0]);

//	click_chatter("test anno: %d", XIA_PAINT_ANNO(p));	//check if anno gives same value as iface

	// HMAC
	//unsigned char hmac[20];
	unsigned int hmac_len = 20;

	HMAC(router_secret, router_secret_length, (unsigned char *)&(challenge.body), sizeof(challenge.body), challenge.hmac, &hmac_len);
	//for(i=0; i<5; i++)
	//	click_chatter("HMAC: %02X", challenge.hmac[i]);

	// Make the challenge packet header
	WritablePacket *challenge_p = Packet::make(256, &challenge, sizeof(struct click_xia_challenge), 0);
	XIAHeaderEncap encap;
	encap.set_nxt(CLICK_XIA_NXT_XCHAL);
	encap.set_dst_path(src_dag);
	encap.set_src_path(_src_path);	// self addr // TODO: do something smarter here (e.g., don't include original dest SID)

	String hid((char*) challenge.body.src_hid);
	XID source_hid(hid);
	challenge_p->set_nexthop_neighbor_xid_anno(source_hid);
//	click_chatter("Self dag: %s\n=========================", _src_path.unparse().c_str() );
	output(1).push(encap.encap(challenge_p));
}

void
XIAChallengeSource::push(int in_port, Packet *p_in)
{
    if(_active) {
		if(in_port == 0) { // Packet from network
			// && from the same AD (need to pass in $local_addr) -- not needed because in_port is never the one from xrc?
			if (p_in->xia_header()->nxt == CLICK_XIA_NXT_XRESP) {
				click_chatter("%s> Response received", _name);
				verify_response(p_in);
				p_in->kill();	// done verifying. drop response packet
				return;
			}
			else if (is_verified(p_in)) {
				// Just forward the packet
				click_chatter("%s>   PASSED: Just forward", _name);
				output(in_port).push(p_in);
				return;
			}
			else {
				click_chatter("%s>   FAILED: This HID is not yet verified. Sending challenge...", _name);
				send_challenge(p_in);
				p_in->kill();	// drop unverified packet
				return;
			}
		}
    }

    // Just forward the packet
    output(in_port).push(p_in);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(XIAChallengeSource)
ELEMENT_MT_SAFE(XIAChallengeSource)
