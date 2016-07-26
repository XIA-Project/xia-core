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

bool build_migrate_message(XIASecurityBuffer &migrate_msg,
        XIAPath &src_path, XIAPath &dst_path)
{
    XIASecurityBuffer migrate_payload(512);

    // Build the migrate payload
    if(!_build_migrate_payload(migrate_payload, src_path, dst_path)) {
        click_chatter("Failed building migrate message payload");
        return false;
    }
    uint8_t *payload = (uint8_t *) migrate_payload.get_buffer();
    uint16_t payloadlen = migrate_payload.size();

    // Find the src XID whose credentials will be used for signing
    XID src_xid = src_path.xid(src_path.destination_node());
    String src_xid_str = src_xid.unparse();

    // Sign the migrate message
    uint8_t signature[MAX_SIGNATURE_SIZE];
    uint16_t siglen = MAX_SIGNATURE_SIZE;
    if(xs_sign(src_xid_str.c_str(), payload, payloadlen, signature, &siglen)) {
        click_chatter("click_chatter: Signing migrate message");
        return false;
    }

    // Extract the pubkey for inclusion in migrate message
    char pubkey[MAX_PUBKEY_SIZE];
    uint16_t pubkeylen = MAX_PUBKEY_SIZE;
    if(xs_getPubkey(src_xid_str.c_str(), pubkey, &pubkeylen)) {
        click_chatter("click_chatter: Pubkey not found:%s", src_xid_str.c_str());
        return false;
    }

    // Fill in the migrate message
    if(migrate_msg.pack((const char *) payload, payloadlen)) {
        click_chatter("Failed packing migrate payload into migrate message");
        return false;
    }
    if(migrate_msg.pack((const char *) signature, siglen)) {
        click_chatter("Failed packing signature into migrate message");
        return false;
    }
    if(migrate_msg.pack((const char *) pubkey, pubkeylen)) {
        click_chatter("Failed packing pubkey into migrate message");
        return false;
    }

    return true;
}

bool _unpack_migrate_payload(const char *payload, uint16_t payloadlen)
{
    uint16_t our_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t their_dag_len = XIA_MAX_DAG_STR_SIZE;
    uint16_t timestamplen = MAX_TIMESTAMP_STR_SIZE;

    char our_dag[our_dag_len];
    char their_dag[their_dag_len];
    char timestamp[timestamplen];

    XIASecurityBuffer migrate_payload(payload, payloadlen);

    if(! migrate_payload.unpack(their_dag, &their_dag_len)) {
        click_chatter("Failed to find their dag in migrate payload");
        return false;
    }
    if(! migrate_payload.unpack(our_dag, &our_dag_len)) {
        click_chatter("Failed to find our dag in migrate payload");
        return false;
    }
    if(! migrate_payload.unpack(timestamp, &timestamplen)) {
        click_chatter("Failed to find timestamp in migrate payload");
        return false;
    }

    return true;
}

bool _unpack_and_verify_migrate(XIASecurityBuffer &migrate_msg)
{
    char payload[512];
    uint16_t payloadlen;

    char signature[MAX_SIGNATURE_SIZE];
    uint16_t siglen;

    char pubkey[MAX_PUBKEY_SIZE];
    uint16_t pubkeylen;

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

    if(! _unpack_migrate_payload(payload, payloadlen)) {
        click_chatter("Failed to extract migrate payload");
        return false;
    }

    return true;
}

bool process_migrate_message(XIASecurityBuffer &migrate_msg)
{
    // Unpack and verify
    if (! _unpack_and_verify_migrate(migrate_msg)) {
        click_chatter("Failed to unpack or verify migrate message");
        return false;
    }
    // Create migrate ack

    return true;
}
