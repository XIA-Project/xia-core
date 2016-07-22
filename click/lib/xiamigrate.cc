#include<xiasecurity.hh>

bool _pack_String(XIASecurityBuffer &buf, String data)
{
    int datalen = strlen(data.c_str()) + 1;
    buf.pack(data.c_str(), datalen);
    return true;
}

bool _build_migrate_payload(XIASecurityBuffer &migrate_payload, sock *sk)
{

    if(!_pack_String(migrate_payload, sk->src_path.unparse())) {
        ERROR("Failed packing src_path into migrate message");
        return false;
    }

    if(!_pack_String(migrate_payload, sk->dst_path.unparse())) {
        ERROR("Failed packing dst_path into migrate message");
        return false;
    }

    Timestamp now = Timestamp::now();
    if(!_pack_String(now.unparse())) {
        ERROR("Failed packing timestamp into migrate message");
        return false;
    }

    return true;
}

bool build_migrate_message(XIASecurityBuffer &migrate_msg, sock *sk)
{
    XIASecurityBuffer migrate_payload(512);

    // Build the migrate payload
    if(!_build_migrate_payload(migrate_payload, sk)) {
        ERROR("Failed building migrate message payload");
        return false;
    }
    uint8_t *payload = migrate_payload.get_buffer();
    uint16_t payloadlen = migrate_payload.size();

    // Find the src XID whose credentials will be used for signing
    XID src_xid = sk->src_path.xid(sk->src_path.destination_node());
    String src_xid_str = src_xid.unparse();
    uint16_t src_xid_strlen = src_xid_str.size() + 1;

    // Sign the migrate message
    uint8_t signature[MAX_SIGNATURE_SIZE];
    uint16_t siglen = MAX_SIGNATURE_SIZE;
    if(xs_sign(src_xid_str.c_str(), payload, payloadlen, signature, siglen)) {
        ERROR("ERROR: Signing migrate message");
        return false;
    }

    // Extract the pubkey for inclusion in migrate message
    char pubkey[MAX_PUBKEY_SIZE];
    uint16_t pubkeylen = MAX_PUBKEY_SIZE;
    if(xs_getPubkey(src_xid_str.c_str(), pubkey, &pubkeylen)) {
        ERROR("ERROR: Pubkey not found:%s", src_xid_str.c_str());
        return false;
    }

    // Fill in the migrate message
    if(migrate_msg.pack(payload, payloadlen)) {
        ERROR("Failed packing migrate payload into migrate message");
        return false;
    }
    if(migrate_msg.pack(signature, siglen)) {
        ERROR("Failed packing signature into migrate message");
        return false;
    }
    if(migrate_msg.pack(pubkey, pubkeylen)) {
        ERROR("Failed packing pubkey into migrate message");
        return false;
    }

    return true;
}

