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
#include <fcntl.h>
#include <unistd.h>
#include <click/config.h>
#include "xiachallengesource.hh"
#include <click/nameinfo.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <click/xid.hh>
#include <click/xiaheader.hh>
#include <click/xiasecurity.hh>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/hmac.h> // HMAC()


CLICK_DECLS


#define CLICK_XIA_NXT_XCHAL        65    /*  Challenge */
#define CLICK_XIA_NXT_XRESP        66    /*  Response */


// NOTE:
// Challenge = ChallengeComponents, HMAC
// ChallengeComponents = srcHID, dstHID, interface, originalPacketHash
// Response = Challenge, Signature, ResponderPubkey
//

/*
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
    uint8_t hmac[20];    // HMAC-SHA1(body, secret)
};

// XIA challenge reply header
struct click_xia_response {
    struct click_xia_challenge v;
    uint8_t pub_key[274];
    uint8_t signature[128];

};
*/


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
    if (cp_va_kparse(conf, this, errh,
                     "INTERFACE", cpkP + cpkM, cpInteger, &_iface,
                     "ACTIVE", 0, cpInteger, &_active,
                   cpEnd) < 0)
        return -1; // error config

    if (!generate_secret()) {
        errh->error("Unable to generate router_secret: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int
XIAChallengeSource::initialize(ErrorHandler *)
{
    return 0;
}

enum {DAG, HID};

int XIAChallengeSource::write_param(const String &conf, Element *e, void *vparam, ErrorHandler *errh)
{
    XIAChallengeSource *f = static_cast<XIAChallengeSource *>(e);
    switch(reinterpret_cast<intptr_t>(vparam)) {
    case DAG:
    {
        XIAPath dag;
        if (cp_va_kparse(conf, f, errh,
                         "ADDR", cpkP + cpkM, cpXIAPath, &dag,
                         cpEnd) < 0)
            return -1;
        f->_src_path = dag;
        click_chatter("XIAChallengeSource: DAG is now %s", f->_src_path.unparse().c_str());
        break;

    }
    case HID:
    {
        XID hid;
        if (cp_va_kparse(conf, f, errh,
                    "HID", cpkP + cpkM, cpXID, &hid, cpEnd) < 0)
            return -1;
        f->_hid = hid;
        String local_hid_str = f->_hid.unparse();
        f->_name = new char [local_hid_str.length()+1];
        strcpy(f->_name, local_hid_str.c_str());
        click_chatter("XIAChallengeSource: HID assigned: %s", f->_hid.unparse().c_str());
        break;
    }
    default:
        break;
    }
    return 0;
}

void XIAChallengeSource::add_handlers()
{
    //add_write_handler("src_path", write_param, (void *)H_MOVE);
    add_write_handler("dag", write_param, (void *)DAG);
    add_write_handler("hid", write_param, (void *)HID);
}

bool
XIAChallengeSource::generate_secret()
{
    int r  = open("/dev/urandom", O_RDONLY);
    int rc = read(r, router_secret, router_secret_length);
    close(r);
    return (rc == router_secret_length);
}

int
XIAChallengeSource::digest_to_hex_string(unsigned char *digest, int digest_len, char *hex_string, int hex_string_len)
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

// NOTE:
// Challenge = ChallengeComponents, HMAC
// ChallengeComponents = srcHID, dstHID, interface, originalPacketHash
// Response = Challenge, Signature, ResponderPubkey
//

void
XIAChallengeSource::verify_response(Packet *p_in)
{
    uint16_t length;

    // Get the src and dst addresses
    XIAHeader xiah(p_in->xia_header());
    XIAPath src_dag = xiah.src_path();
    XIAPath dst_dag = xiah.dst_path();


    // Extract Response from payload: Challenge, Signature, ResponderPubkey
    XIASecurityBuffer response((char *)xiah.payload(), xiah.plen());
    if(response.get_numEntries() != 3) {
        click_chatter("XIAChallengeSource::verify_response: ERROR: Expected 3 entries, got %d", response.get_numEntries());
        return;
    }

    // Extract Challenge from Response
    length = response.peekUnpackLength();
    char challengeBlob[length];
    response.unpack((char *)challengeBlob, &length);
    XIASecurityBuffer challenge(challengeBlob, length);

    // Extract Signature from Response
    length = response.peekUnpackLength();
    char responseSignature[length];
    response.unpack((char *)responseSignature, &length);

    // Extract Remote Host's Public key from Response
    length = response.peekUnpackLength();
    char remotePubkey[length+1]; // null terminate
    bzero(remotePubkey, length+1);
    response.unpack((char *)remotePubkey, &length);

    // Extract ChallengeComponents from Challenge
    length = challenge.peekUnpackLength();
    char challengeComponents[length];
    challenge.unpack((char *)challengeComponents, &length);
    XIASecurityBuffer chalComponents(challengeComponents, length);

    // Extract HMAC from Challenge
    length = challenge.peekUnpackLength();
    char hmac[length];
    challenge.unpack((char *)hmac, &length);

    // Extract src_hid from ChallengeComponents
    length = chalComponents.peekUnpackLength();
    char challenge_src_hid[length+1]; // null terminate
    bzero(challenge_src_hid, length+1);
    chalComponents.unpack((char *)challenge_src_hid, &length);

    // Extract dst_hid from ChallengeComponents
    length = chalComponents.peekUnpackLength();
    char challenge_dst_hid[length+1]; // null terminate
    bzero(challenge_dst_hid, length+1);
    chalComponents.unpack((char *)challenge_dst_hid, &length);

    // Extract interface from ChallengeComponents
    int interface;
    length = sizeof(int);
    chalComponents.unpack((char *)&interface, &length);

    // Hash of packet that was originally sent
    length = chalComponents.peekUnpackLength();
    char sentPktHash[length];
    chalComponents.unpack((char *)sentPktHash, &length);

    // Print out debugging info about the response packet
    click_chatter("== Response Packet detail ============================================================");
    click_chatter("\tSRC:%s:", challenge_src_hid);
    click_chatter("\tDST:%s:", challenge_dst_hid);
    click_chatter("\tInterface:%d", interface);

    // Verify HMAC: Check if the challenge message really originated from here
    unsigned int hmac_len = 20;
    unsigned char challenge_hmac[hmac_len];
    HMAC(EVP_sha1(), router_secret, router_secret_length, (unsigned char *)chalComponents.get_buffer(), chalComponents.size(), challenge_hmac, &hmac_len);

    if(memcmp(hmac, challenge_hmac, hmac_len)) {
        click_chatter("%s> Error: Invalid hmac returned with response", _name);
        return;
    }

    // Verify public key: Check if hash(pub) == src_hid
    String src_hid(src_hid_str(p_in));
    if(!xs_pubkeyMatchesXID(remotePubkey, src_hid.c_str())) {
        click_chatter("%s> ERROR: %s does not match public key in response", src_hid.c_str());
        return;
    }
    click_chatter("%s> Public key matches source HID", _name);

    // Verify the signature from the original packet sender
    if(xs_isValidSignature((const unsigned char *)challenge.get_buffer(), challenge.size(), (unsigned char *)responseSignature, sizeof responseSignature, remotePubkey, sizeof remotePubkey) != 1) {
        click_chatter("%s> XIAChallengeSource::verify_challenge: Invalid signature", _name);
    }

    click_chatter("%s> Signature verified. Accept this source HID", _name);

    _verified_table[src_hid] = 1;

}

String
XIAChallengeSource::src_hid_str(Packet *p)
{
    XIAHeader xiah(p->xia_header());
    XIAPath src_dag = xiah.src_path();
    return (src_dag.intent_hid_str()).c_str();
}

// Check whether pack/et's source HID has been verified
bool
XIAChallengeSource::is_verified(Packet *p)
{
    String hid(src_hid_str(p));
    click_chatter("%s> Verifying %s", _name, hid.c_str());

    return _verified_table[hid] == 1;
}


// send a challenge packet to the source of the packet
void
XIAChallengeSource::send_challenge(Packet *p)
{
    // Get the src and dst addresses
    XIAHeader xiah(p->xia_header());
    XIAPath src_dag = xiah.src_path();
    XIAPath dst_dag = xiah.dst_path();

    // Challenge includes
    // buf = src_hid + dst_hid + iface + pkt_hash
    // challenge = buf + hmac(buf)

    // Make the challenge packet payload
    XIASecurityBuffer buf = XIASecurityBuffer(128);

    // Source HID
    String src_hid_str = (src_dag.intent_hid_str()).c_str();
    buf.pack(src_hid_str.c_str(), src_hid_str.length());

    // Destination HID
    String dst_hid_str = (dst_dag.intent_hid_str()).c_str();
    buf.pack(dst_hid_str.c_str(), dst_hid_str.length());

    // Interface
    buf.pack((const char *)&_iface, sizeof(_iface));

    // Hash of incoming packet
    unsigned char pkt_hash[SHA_DIGEST_LENGTH];
    xs_getSHA1Hash(p->data(), p->length(), pkt_hash, sizeof pkt_hash);
    buf.pack((const char *)pkt_hash, sizeof pkt_hash);

    // HMAC computed on all abovementioned entries
    unsigned char challenge_hmac[20];
    unsigned int hmac_len = 20;
    HMAC(EVP_sha1(), router_secret, router_secret_length, (unsigned char *)buf.get_buffer(), buf.size(), challenge_hmac, &hmac_len);

    // Build the challenge
    XIASecurityBuffer challenge = XIASecurityBuffer(512);
    challenge.pack(buf.get_buffer(), buf.size());
    challenge.pack((const char *)challenge_hmac, hmac_len);


    // Make the challenge packet header
    WritablePacket *challenge_p = Packet::make(256, challenge.get_buffer(), challenge.size(), 0);
    XIAHeaderEncap encap;
    encap.set_nxt(CLICK_XIA_NXT_XCHAL);
    encap.set_dst_path(src_dag);
    encap.set_src_path(_src_path);    // self addr // TODO: do something smarter here (e.g., don't include original dest SID)

    XID source_hid(src_hid_str);
    challenge_p->set_nexthop_neighbor_xid_anno(source_hid);
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
                p_in->kill();    // done verifying. drop response packet
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
                p_in->kill();    // drop unverified packet
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
ELEMENT_LIBS(-lcrypto -lssl)
