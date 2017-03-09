#include<strings.h>
#include<click/config.h>
#include<click/xiautil.hh>
#include<click/xiarendezvous.hh>
#include<click/xiasecurity.hh>

bool _build_rv_control_payload(XIASecurityBuffer &rv_control_payload,
		String &hid, String &dag, String &timestamp)
{

    if(!rv_control_payload.pack(hid.c_str())) {
        click_chatter("Failed packing src_path into rv_control message");
        return false;
    }

    if(!rv_control_payload.pack(dag.c_str())) {
        click_chatter("Failed packing dst_path into rv_control message");
        return false;
    }

    if(!rv_control_payload.pack(timestamp.c_str())) {
        click_chatter("Failed packing timestamp into rv_control message");
        return false;
    }

    return true;
}

/*
bool _sign_and_pack(XIASecurityBuffer &buf, XIASecurityBuffer &payloadbuf,
        std::string xid_str)
{

    unsigned char *payload = (unsigned char *)payloadbuf.get_buffer();
    uint16_t payloadlen = payloadbuf.size();

    // Sign the rv_control message
    uint8_t signature[MAX_SIGNATURE_SIZE];
    uint16_t siglen = MAX_SIGNATURE_SIZE;
    if(xs_sign(xid_str.c_str(), payload, payloadlen, signature, &siglen)) {
        click_chatter("click_chatter: Signing rv_control message");
        return false;
    }

    // Extract the pubkey for inclusion in rv_control message
    char pubkey[MAX_PUBKEY_SIZE];
    uint16_t pubkeylen = MAX_PUBKEY_SIZE;
    if(xs_getPubkey(xid_str.c_str(), pubkey, &pubkeylen)) {
        click_chatter("click_chatter: Pubkey not found:%s", xid_str.c_str());
        return false;
    }

    // Fill in the rv_control message
    if(!buf.pack((const char *) payload, payloadlen)) {
        click_chatter("Failed packing rv_control payload into rv_control message");
        return false;
    }
    if(!buf.pack((const char *) signature, siglen)) {
        click_chatter("Failed packing signature into rv_control message");
        return false;
    }
    if(!buf.pack((const char *) pubkey, pubkeylen)) {
        click_chatter("Failed packing pubkey into rv_control message");
        return false;
    }

    return true;
}
*/

bool build_rv_control_message(XIASecurityBuffer &rv_control_msg,
		String &hid, String &dag, String &timestamp)
{
    XIASecurityBuffer rv_control_payload(512);

    // Build the rendezvous control message payload
    if(!_build_rv_control_payload(rv_control_payload, hid, dag, timestamp)) {
        click_chatter("Failed building rv control message payload");
        return false;
    }

    // Sign and include pubkey of HID into rv_control message
    if(!rv_control_msg.sign_and_pack(rv_control_payload, hid.c_str())) {
        click_chatter("Failed to sign and create rv control message");
        return false;
    }

    return true;
}

/*
bool _unpack_rv_control_payload(XIASecurityBuffer &rv_control_payload,
        char *our_dag, uint16_t *our_dag_len,
        char *their_dag, uint16_t *their_dag_len,
        char *timestamp, uint16_t *timestamplen)
{

    if(! rv_control_payload.unpack(their_dag, their_dag_len)) {
        click_chatter("Failed to find their dag in rv_control payload");
        return false;
    }

    if(! rv_control_payload.unpack(our_dag, our_dag_len)) {
        click_chatter("Failed to find our dag in rv_control payload");
        return false;
    }

    if(! rv_control_payload.unpack(timestamp, timestamplen)) {
        click_chatter("Failed to find timestamp in rv_control payload");
        return false;
    }

    return true;
}

bool _unpack_and_verify(XIASecurityBuffer &buf, std::string xid_str,
        char *payload, uint16_t *payloadlen)
{
    uint16_t siglen = MAX_SIGNATURE_SIZE;
    char signature[siglen];

    uint16_t pubkeylen = MAX_PUBKEY_SIZE;
    char pubkey[pubkeylen];
    bzero((void *)pubkey, (size_t)pubkeylen);

    if(! buf.unpack(payload, payloadlen)) {
        click_chatter("Failed to unpack payload from rv_control message");
        return false;
    }

    if(! buf.unpack(signature, &siglen)) {
        click_chatter("Failed to extract signature from rv_control message");
        return false;
    }

    if(! buf.unpack(pubkey, &pubkeylen)) {
        click_chatter("Failed to extract public key from rv_control message");
        return false;
    }

    // Public key must match the XID
    if(!xs_pubkeyMatchesXID(pubkey, xid_str.c_str())) {
        click_chatter("ERROR: Mismatched rv_control XID and pubkey");
        return false;
    }

    // and signature is valid against the pubkey
    if(!xs_isValidSignature((const unsigned char *)payload, *payloadlen,
                (unsigned char *)signature, siglen,
                pubkey, pubkeylen)) {
        click_chatter("ERROR: Invalid signature in rv_control message");
        return false;
    }

    return true;
}

bool _verify_rv_control(XIASecurityBuffer &rv_control_payload,
        XIAPath their_addr, XIAPath our_addr,
        XIAPath &their_new_addr,
		String &rv_control_ts)
{
    // The payload contains [their_new_dag, our_dag, timestamp]
    uint16_t our_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t their_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t timestamplen = MAX_RV_TIMESTAMP_STR_SIZE;

    char our_dag_str[our_dag_len];
    char their_dag_str[their_dag_len];
    char timestamp[timestamplen];

    if(! _unpack_rv_control_payload(rv_control_payload,
                our_dag_str, &our_dag_len,
                their_dag_str, &their_dag_len,
                timestamp, &timestamplen)) {
        click_chatter("Failed to extract rv_control payload");
        return false;
    }

    // Their intent XID from their address in XIP header
    XID their_xid = their_addr.xid(their_addr.destination_node());

    // must match intent XID in their_dag included in rv_control
    XIAPath their_dag;
    if(their_dag.parse(their_dag_str) == false) {
        click_chatter("ERROR: Unable to parse their dag in rv_control message");
        return false;
    }
    XID rv_control_xid = their_dag.xid(their_dag.destination_node());
    if(rv_control_xid != their_xid) {
        click_chatter("ERROR: Mismatched rv_control XID");
        return false;
    }

    // Our intent XID from our address in XIP header
    XID our_xid = our_addr.xid(our_addr.destination_node());

    // must match intent XID in our_dag included in rv_control
    XIAPath our_dag;
    if(our_dag.parse(our_dag_str) == false) {
        click_chatter("ERROR: Unable to parse our dag in rv_control message");
        return false;
    }
    XID our_xid_in_rv_control = our_dag.xid(our_dag.destination_node());
    if(our_xid_in_rv_control != our_xid) {
        click_chatter("ERROR: Mismatched XID for us");
        return false;
    }

    their_new_addr = their_dag;
	rv_control_ts = timestamp;
    return true;
}

bool valid_rv_control_message(XIASecurityBuffer &rv_control_msg,
        XIAPath their_addr, XIAPath our_addr,
        XIAPath &accepted_addr, String &rv_control_ts)
{
    XIAPath their_new_addr;

    uint16_t payloadlen = 1024;
    char payload[payloadlen];
	std::string sender_xid = their_addr.intent_sid_str();

    // Unpack and verify signature, then return payload
    if(! _unpack_and_verify(rv_control_msg, sender_xid, payload, &payloadlen)) {
        click_chatter("Failed to unpack/verify rv_control payload");
        return false;
    }
    XIASecurityBuffer rv_control_payload(payload, payloadlen);

    // Unpack and verify
    if (! _verify_rv_control(rv_control_payload, their_addr, our_addr,
                their_new_addr, rv_control_ts)) {
        click_chatter("Failed to unpack or verify rv_control message");
        return false;
    }

    // Update their address with new one in rv_control packet
    accepted_addr = their_new_addr;

    return true;
}

bool _build_rv_controlack_payload(XIASecurityBuffer &rv_controlack_payload,
        XIAPath their_addr, String timestamp)
{
    // their_addr is the new DAG we have accepted
    if(!_pack_String(rv_controlack_payload, their_addr.unparse())) {
        click_chatter("Failed packing their addr into rv_controlack message");
        return false;
    }

    if(!_pack_String(rv_controlack_payload, timestamp)) {
        click_chatter("Failed to pack timestamp into rv_controlack message");
        return false;
    }
    return true;
}

bool build_rv_controlack_message(XIASecurityBuffer &rv_controlack_msg,
        XIAPath our_addr, XIAPath their_addr, String timestamp)
{
    XIASecurityBuffer rv_controlack_payload(512);

    // Build the payload
    if(!_build_rv_controlack_payload(rv_controlack_payload,
                their_addr, timestamp)) {
        click_chatter("ERROR: Failed building rv_controlack payload");
        return false;
    }

	std::string my_xid_str = our_addr.intent_sid_str();

    // Sign and include public key
    if(!_sign_and_pack(rv_controlack_msg, rv_controlack_payload, my_xid_str)) {
        click_chatter("Failed to sign and create rv_control message");
        return false;
    }

    return true;
}

bool _unpack_rv_controlack_payload(XIASecurityBuffer &rv_controlack_payload,
        char *our_dag, uint16_t *our_dag_len,
        char *timestamp, uint16_t *timestamplen)
{

    if(! rv_controlack_payload.unpack(our_dag, our_dag_len)) {
        click_chatter("Failed to find our dag in rv_controlack payload");
        return false;
    }

    if(! rv_controlack_payload.unpack(timestamp, timestamplen)) {
        click_chatter("Failed to find timestamp in rv_controlack payload");
        return false;
    }

    return true;
}

bool _verify_rv_controlack(XIASecurityBuffer &rv_controlack_payload,
        XIAPath our_addr, String timestamp)
{
    // The payload contains [our_new_dag, timestamp]
    uint16_t our_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t timestamplen = MAX_TIMESTAMP_STR_SIZE;

    char our_dag_str[our_dag_len];
    char msgtimestamp[timestamplen];

    if(! _unpack_rv_controlack_payload(rv_controlack_payload,
                our_dag_str, &our_dag_len,
                msgtimestamp, &timestamplen)) {
        click_chatter("Failed to extract rv_controlack payload");
        return false;
    }

    // Our intent XID from our address in XIP header
    XID our_xid = our_addr.xid(our_addr.destination_node());

    // must match intent XID in our_dag included in rv_controlack
    XIAPath our_dag;
    if(our_dag.parse(our_dag_str) == false) {
        click_chatter("ERROR: Unable to parse our dag in rv_controlack message");
        return false;
    }
    XID our_xid_in_rv_controlack = our_dag.xid(our_dag.destination_node());
    if(our_xid_in_rv_controlack != our_xid) {
        click_chatter("ERROR: Mismatched XID for us");
        return false;
    }

    // Verify that the timestamp matches the one from rv_controlack
    String msgtimestampstr(msgtimestamp);
    if(timestamp.compare(msgtimestampstr)) {
        click_chatter("ERROR: Mismatched timestamp");
        return false;
    }

    return true;
}

bool valid_rv_controlack_message(XIASecurityBuffer &rv_controlack_msg,
        XIAPath their_addr, XIAPath our_addr, String timestamp)
{
    uint16_t payloadlen = 1024;
    char payload[payloadlen];
	std::string their_xid = their_addr.intent_sid_str();

    // Unpack and verify signature, then return payload
    if(! _unpack_and_verify(rv_controlack_msg, their_xid, payload, &payloadlen)) {
        click_chatter("Failed to unpack/verify rv_controlack payload");
        return false;
    }
    XIASecurityBuffer rv_controlack_payload(payload, payloadlen);

    // Unpack and verify
    if (! _verify_rv_controlack(rv_controlack_payload, our_addr, timestamp)) {
        click_chatter("Failed to unpack or verify rv_controlack message");
        return false;
    }

    return true;
}
*/
