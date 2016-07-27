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
        XIAPath &src_path, XIAPath &dst_path)
{

    if(!_pack_String(migrate_payload, src_path.unparse())) {
        click_chatter("Failed packing src_path into migrate message");
        return false;
    }

    if(!_pack_String(migrate_payload, dst_path.unparse())) {
        click_chatter("Failed packing dst_path into migrate message");
        return false;
    }

    Timestamp now = Timestamp::now();
    if(!_pack_String(migrate_payload, now.unparse())) {
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
    if(buf.pack((const char *) payload, payloadlen)) {
        click_chatter("Failed packing migrate payload into migrate message");
        return false;
    }
    if(buf.pack((const char *) signature, siglen)) {
        click_chatter("Failed packing signature into migrate message");
        return false;
    }
    if(buf.pack((const char *) pubkey, pubkeylen)) {
        click_chatter("Failed packing pubkey into migrate message");
        return false;
    }

    return true;
}

bool build_migrate_message(XIASecurityBuffer &migrate_msg,
        XIAPath &src_path, XIAPath &dst_path)
{
    XIASecurityBuffer migrate_payload(512);

    // Build the migrate payload
    if(!_build_migrate_payload(migrate_payload, src_path, dst_path)) {
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

bool _unpack_migrate_payload(const char *payload, uint16_t payloadlen,
        char *our_dag, uint16_t *our_dag_len,
        char *their_dag, uint16_t *their_dag_len,
        char *timestamp, uint16_t *timestamplen)
{
    XIASecurityBuffer migrate_payload(payload, payloadlen);

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

bool _unpack_and_verify_migrate(XIASecurityBuffer &migrate_msg,
        XIAPath their_addr, XIAPath our_addr,
        XIAPath &their_new_addr)
{
    uint16_t payloadlen = 1024;
    char payload[payloadlen];

    uint16_t siglen = MAX_SIGNATURE_SIZE;
    char signature[siglen];

    uint16_t pubkeylen = MAX_PUBKEY_SIZE;
    char pubkey[pubkeylen];
    bzero((void *)pubkey, (size_t)pubkeylen);

    if(! migrate_msg.unpack(payload, &payloadlen)) {
        click_chatter("Failed to unpack payload from migrate message");
        return false;
    }

    if(! migrate_msg.unpack(signature, &siglen)) {
        click_chatter("Failed to extract signature from migrate message");
        return false;
    }

    if(! migrate_msg.unpack(pubkey, &pubkeylen)) {
        click_chatter("Failed to extract public key from migrate message");
        return false;
    }

    // The payload contains [their_new_dag, our_dag, timestamp]
    uint16_t our_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t their_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t timestamplen = MAX_TIMESTAMP_STR_SIZE;

    char our_dag_str[our_dag_len];
    char their_dag_str[their_dag_len];
    char timestamp[timestamplen];

    if(! _unpack_migrate_payload(payload, payloadlen,
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
    if (migrate_xid.unparse().compare(their_xid.unparse())) {
        click_chatter("ERROR: Mismatched migrate XID");
        return false;
    }

    // and should match the public key included in migrate
    if(!xs_pubkeyMatchesXID(pubkey, their_xid.unparse().c_str())) {
        click_chatter("ERROR: Mismatched migrate XID and pubkey");
        return false;
    }

    // and signature is valid
    if(!xs_isValidSignature((const unsigned char *)payload, payloadlen,
                (unsigned char *)signature, siglen,
                their_xid.unparse().c_str() )) {
        click_chatter("ERROR: Invalid signature in migrate message");
        return false;
    }

    their_new_addr = their_dag;
    return true;
}

bool valid_migrate_message(XIASecurityBuffer &migrate_msg,
        XIAPath their_addr, XIAPath our_addr,
        XIAPath &accepted_addr)
{
    XIAPath their_new_addr;

    // Unpack and verify
    if (! _unpack_and_verify_migrate(migrate_msg, their_addr, our_addr,
                their_new_addr)) {
        click_chatter("Failed to unpack or verify migrate message");
        return false;
    }

    // Update their address with new one in migrate packet
    accepted_addr = their_new_addr;

    return true;
}

bool _build_migrateack_payload(XIASecurityBuffer &migrateack_payload,
        XIAPath our_addr, XIAPath their_addr, String timestamp)
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
                our_addr, their_addr, timestamp)) {
        click_chatter("ERROR: Failed building migrateack payload");
        return false;
    }

    XID my_xid = our_addr.xid(our_addr.destination_node());
    // Sign and include public key
    if(!_sign_and_pack(migrateack_msg, migrateack_payload, my_xid)) {
        click_chatter("Failed to sign and create migrate message");
        return false;
    }

    return true;
}
