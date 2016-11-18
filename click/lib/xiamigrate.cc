#include<strings.h>
#include<click/config.h>
#include<click/xiautil.hh>
#include<click/xiamigrate.hh>
#include<click/xiasecurity.hh>

bool _pack_String(XIASecurityBuffer &buf, String data)
{
    int datalen = strlen(data.c_str()) + 1;
    buf.pack(data.c_str(), datalen);
    return true;
}

bool _build_migrate_payload(XIASecurityBuffer &migrate_payload,
        XIAPath &src_path, XIAPath &dst_path, String &migrate_ts)
{

    if(!_pack_String(migrate_payload, src_path.unparse())) {
        click_chatter("Failed packing src_path into migrate message");
        return false;
    }

    if(!_pack_String(migrate_payload, dst_path.unparse())) {
        click_chatter("Failed packing dst_path into migrate message");
        return false;
    }

    if(!_pack_String(migrate_payload, migrate_ts)) {
        click_chatter("Failed packing timestamp into migrate message");
        return false;
    }

    return true;
}

bool _sign_and_pack(XIASecurityBuffer &buf, XIASecurityBuffer &payloadbuf,
        XID xid)
{

    unsigned char *payload = (unsigned char *)payloadbuf.get_buffer();
    uint16_t payloadlen = payloadbuf.size();

    // Find the src XID whose credentials will be used for signing
    String xid_str = xid.unparse();

    // Sign the migrate message
    uint8_t signature[MAX_SIGNATURE_SIZE];
    uint16_t siglen = MAX_SIGNATURE_SIZE;
    if(xs_sign(xid_str.c_str(), payload, payloadlen, signature, &siglen)) {
        click_chatter("click_chatter: Signing migrate message");
        return false;
    }

    // Extract the pubkey for inclusion in migrate message
    char pubkey[MAX_PUBKEY_SIZE];
    uint16_t pubkeylen = MAX_PUBKEY_SIZE;
    if(xs_getPubkey(xid_str.c_str(), pubkey, &pubkeylen)) {
        click_chatter("click_chatter: Pubkey not found:%s", xid_str.c_str());
        return false;
    }

    // Fill in the migrate message
    if(!buf.pack((const char *) payload, payloadlen)) {
        click_chatter("Failed packing migrate payload into migrate message");
        return false;
    }
    if(!buf.pack((const char *) signature, siglen)) {
        click_chatter("Failed packing signature into migrate message");
        return false;
    }
    if(!buf.pack((const char *) pubkey, pubkeylen)) {
        click_chatter("Failed packing pubkey into migrate message");
        return false;
    }

    return true;
}

bool build_migrate_message(XIASecurityBuffer &migrate_msg,
        XIAPath &src_path, XIAPath &dst_path, String &migrate_ts)
{
    XIASecurityBuffer migrate_payload(512);

    // Build the migrate payload
    if(!_build_migrate_payload(migrate_payload, src_path, dst_path,
				migrate_ts)) {
        click_chatter("Failed building migrate message payload");
        return false;
    }

    // Sign and include pubkey into migrate message
    XID src_xid = src_path.xid(src_path.destination_node());

    if(!_sign_and_pack(migrate_msg, migrate_payload, src_xid)) {
        click_chatter("Failed to sign and create migrate message");
        return false;
    }

    return true;
}

bool _unpack_migrate_payload(XIASecurityBuffer &migrate_payload,
        char *our_dag, uint16_t *our_dag_len,
        char *their_dag, uint16_t *their_dag_len,
        char *timestamp, uint16_t *timestamplen)
{

    if(! migrate_payload.unpack(their_dag, their_dag_len)) {
        click_chatter("Failed to find their dag in migrate payload");
        return false;
    }

    if(! migrate_payload.unpack(our_dag, our_dag_len)) {
        click_chatter("Failed to find our dag in migrate payload");
        return false;
    }

    if(! migrate_payload.unpack(timestamp, timestamplen)) {
        click_chatter("Failed to find timestamp in migrate payload");
        return false;
    }

    return true;
}

bool _unpack_and_verify(XIASecurityBuffer &buf, XID xid,
        char *payload, uint16_t *payloadlen)
{
    uint16_t siglen = MAX_SIGNATURE_SIZE;
    char signature[siglen];

    uint16_t pubkeylen = MAX_PUBKEY_SIZE;
    char pubkey[pubkeylen];
    bzero((void *)pubkey, (size_t)pubkeylen);

    if(! buf.unpack(payload, payloadlen)) {
        click_chatter("Failed to unpack payload from migrate message");
        return false;
    }

    if(! buf.unpack(signature, &siglen)) {
        click_chatter("Failed to extract signature from migrate message");
        return false;
    }

    if(! buf.unpack(pubkey, &pubkeylen)) {
        click_chatter("Failed to extract public key from migrate message");
        return false;
    }

    // Public key must match the XID
    if(!xs_pubkeyMatchesXID(pubkey, xid.unparse().c_str())) {
        click_chatter("ERROR: Mismatched migrate XID and pubkey");
        return false;
    }

    // and signature is valid against the pubkey
    if(!xs_isValidSignature((const unsigned char *)payload, *payloadlen,
                (unsigned char *)signature, siglen,
                pubkey, pubkeylen)) {
        click_chatter("ERROR: Invalid signature in migrate message");
        return false;
    }

    return true;
}

bool _verify_migrate(XIASecurityBuffer &migrate_payload,
        XIAPath their_addr, XIAPath our_addr,
        XIAPath &their_new_addr,
		String &migrate_ts)
{
    // The payload contains [their_new_dag, our_dag, timestamp]
    uint16_t our_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t their_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t timestamplen = MAX_TIMESTAMP_STR_SIZE;

    char our_dag_str[our_dag_len];
    char their_dag_str[their_dag_len];
    char timestamp[timestamplen];

    if(! _unpack_migrate_payload(migrate_payload,
                our_dag_str, &our_dag_len,
                their_dag_str, &their_dag_len,
                timestamp, &timestamplen)) {
        click_chatter("Failed to extract migrate payload");
        return false;
    }

    // Their intent XID from their address in XIP header
    XID their_xid = their_addr.xid(their_addr.destination_node());

    // must match intent XID in their_dag included in migrate
    XIAPath their_dag;
    if(their_dag.parse(their_dag_str) == false) {
        click_chatter("ERROR: Unable to parse their dag in migrate message");
        return false;
    }
    XID migrate_xid = their_dag.xid(their_dag.destination_node());
    if(migrate_xid != their_xid) {
        click_chatter("ERROR: Mismatched migrate XID");
        return false;
    }

    // Our intent XID from our address in XIP header
    XID our_xid = our_addr.xid(our_addr.destination_node());

    // must match intent XID in our_dag included in migrate
    XIAPath our_dag;
    if(our_dag.parse(our_dag_str) == false) {
        click_chatter("ERROR: Unable to parse our dag in migrate message");
        return false;
    }
    XID our_xid_in_migrate = our_dag.xid(our_dag.destination_node());
    if(our_xid_in_migrate != our_xid) {
        click_chatter("ERROR: Mismatched XID for us");
        return false;
    }

    their_new_addr = their_dag;
	migrate_ts = timestamp;
    return true;
}

bool valid_migrate_message(XIASecurityBuffer &migrate_msg,
        XIAPath their_addr, XIAPath our_addr,
        XIAPath &accepted_addr, String &migrate_ts)
{
    XIAPath their_new_addr;

    uint16_t payloadlen = 1024;
    char payload[payloadlen];
    XID sender_xid = their_addr.xid(their_addr.destination_node());

    // Unpack and verify signature, then return payload
    if(! _unpack_and_verify(migrate_msg, sender_xid, payload, &payloadlen)) {
        click_chatter("Failed to unpack/verify migrate payload");
        return false;
    }
    XIASecurityBuffer migrate_payload(payload, payloadlen);

    // Unpack and verify
    if (! _verify_migrate(migrate_payload, their_addr, our_addr,
                their_new_addr, migrate_ts)) {
        click_chatter("Failed to unpack or verify migrate message");
        return false;
    }

    // Update their address with new one in migrate packet
    accepted_addr = their_new_addr;

    return true;
}

bool _build_migrateack_payload(XIASecurityBuffer &migrateack_payload,
        XIAPath their_addr, String timestamp)
{
    // their_addr is the new DAG we have accepted
    if(!_pack_String(migrateack_payload, their_addr.unparse())) {
        click_chatter("Failed packing their addr into migrateack message");
        return false;
    }

    if(!_pack_String(migrateack_payload, timestamp)) {
        click_chatter("Failed to pack timestamp into migrateack message");
        return false;
    }
    return true;
}

bool build_migrateack_message(XIASecurityBuffer &migrateack_msg,
        XIAPath our_addr, XIAPath their_addr, String timestamp)
{
    XIASecurityBuffer migrateack_payload(512);

    // Build the payload
    if(!_build_migrateack_payload(migrateack_payload,
                their_addr, timestamp)) {
        click_chatter("ERROR: Failed building migrateack payload");
        return false;
    }

    XID my_xid = our_addr.xid(our_addr.find_intent_sid());
    // Sign and include public key
    if(!_sign_and_pack(migrateack_msg, migrateack_payload, my_xid)) {
        click_chatter("Failed to sign and create migrate message");
        return false;
    }

    return true;
}

bool _unpack_migrateack_payload(XIASecurityBuffer &migrateack_payload,
        char *our_dag, uint16_t *our_dag_len,
        char *timestamp, uint16_t *timestamplen)
{

    if(! migrateack_payload.unpack(our_dag, our_dag_len)) {
        click_chatter("Failed to find our dag in migrateack payload");
        return false;
    }

    if(! migrateack_payload.unpack(timestamp, timestamplen)) {
        click_chatter("Failed to find timestamp in migrateack payload");
        return false;
    }

    return true;
}

bool _verify_migrateack(XIASecurityBuffer &migrateack_payload,
        XIAPath our_addr, String timestamp)
{
    // The payload contains [our_new_dag, timestamp]
    uint16_t our_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t timestamplen = MAX_TIMESTAMP_STR_SIZE;

    char our_dag_str[our_dag_len];
    char msgtimestamp[timestamplen];

    if(! _unpack_migrateack_payload(migrateack_payload,
                our_dag_str, &our_dag_len,
                msgtimestamp, &timestamplen)) {
        click_chatter("Failed to extract migrateack payload");
        return false;
    }

    // Our intent XID from our address in XIP header
    XID our_xid = our_addr.xid(our_addr.destination_node());

    // must match intent XID in our_dag included in migrateack
    XIAPath our_dag;
    if(our_dag.parse(our_dag_str) == false) {
        click_chatter("ERROR: Unable to parse our dag in migrateack message");
        return false;
    }
    XID our_xid_in_migrateack = our_dag.xid(our_dag.destination_node());
    if(our_xid_in_migrateack != our_xid) {
        click_chatter("ERROR: Mismatched XID for us");
        return false;
    }

    // Verify that the timestamp matches the one from migrateack
    String msgtimestampstr(msgtimestamp);
    if(timestamp.compare(msgtimestampstr)) {
        click_chatter("ERROR: Mismatched timestamp");
        return false;
    }

    return true;
}

bool valid_migrateack_message(XIASecurityBuffer &migrateack_msg,
        XIAPath their_addr, XIAPath our_addr, String timestamp)
{
    uint16_t payloadlen = 1024;
    char payload[payloadlen];
    XID their_xid = their_addr.xid(their_addr.find_intent_sid());

    // Unpack and verify signature, then return payload
    if(! _unpack_and_verify(migrateack_msg, their_xid, payload, &payloadlen)) {
        click_chatter("Failed to unpack/verify migrateack payload");
        return false;
    }
    XIASecurityBuffer migrateack_payload(payload, payloadlen);

    // Unpack and verify
    if (! _verify_migrateack(migrateack_payload, our_addr, timestamp)) {
        click_chatter("Failed to unpack or verify migrateack message");
        return false;
    }

    return true;
}
