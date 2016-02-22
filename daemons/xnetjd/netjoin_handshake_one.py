#!/usr/bin/python
#

from google.protobuf import text_format as protobuf_text_format
import struct
import logging
import jacp_pb2
import threading
import nacl.utils
import netjoin_session

# Build a HandshakeOne protobuf in response to a NetDescriptor beacon
class NetjoinHandshakeOne(object):

    def __init__(self, session, mymac):
        self.handshake_one = jacp_pb2.HandshakeOne()
        self.payload = jacp_pb2.HandshakeOne.HandshakeOneEncrypted.Payload()
        self.session = session

        # Now fill in handshake_one
        h1 = self.handshake_one.encrypted
        h1.client_ephemeral_pubkey = session.auth.get_raw_verify_key()

        # Build the payload and add it to h1
        core = self.payload.core
        raw_mac = struct.pack("6B", mymac[0], mymac[1], mymac[2], mymac[3], mymac[4], mymac[5])
        core.client_l2_req.ethernet.client_mac_address = raw_mac
        core.client_l3_req.xip.single.ClientHID = "HID:01234"
        core.client_l3_req.xip.single.configXIP.pxhcp.SetInParent()
        core.client_credentials.null.SetInParent()

    def update_nonce(self):
        h1 = self.handshake_one.encrypted

        # Update the nonce
        h1.nonce = self.session.auth.get_nonce()

        # Update the hash of headers that includes the nonce
        data_to_hash = h1.nonce + h1.client_ephemeral_pubkey
        self.payload.hash_of_headers = self.session.auth.sha512(data_to_hash)

        # Serialize and encrypt the payload
        serialized_payload = self.payload.SerializeToString()
        encrypted_payload = self.session.auth.encrypt(serialized_payload, h1.nonce)

        # Add the encrypted data back to HandshakeOne
        h1.encrypted_data = encrypted_payload

    def handshake_one_str(self):
        return protobuf_text_format.MessageToString(self.handshake_one)

    def print_handshake_one(self):
        print self.handshake_one_str()

    def from_handshake_one(self, handshake_one):
        self.handshake_one.CopyFrom(handshake_one)

    # serialized_handshake_one is actually a serialized jacp_pb2.HandshakeOne
    def from_serialized_handshake_one(self, serialized_handshake_one):
        self.handshake_one.ParseFromString(serialized_handshake_one)

if __name__ == "__main__":
    shutdown_event = threading.Event()
    session = netjoin_session.NetjoinSession(shutdown_event)
    session.daemon = True
    session.start()
    # Hack: setting their verify key same as ours
    session.auth.set_their_raw_verify_key(session.auth.get_raw_verify_key())
    mac = nacl.utils.random(6)
    handshake_one = NetjoinHandshakeOne(session, mac)
    handshake_one.update_nonce()
    serialized_handshake_one = handshake_one.handshake_one.SerializeToString()
    size = len(serialized_handshake_one)
    logging.debug("Serialized handshake one size: {}".format(size))
    handshake_one.print_handshake_one()
    shutdown_event.set()
    print "PASSED: handshake one test"
