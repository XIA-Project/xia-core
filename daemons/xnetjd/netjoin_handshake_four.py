#!/usr/bin/python
#

from google.protobuf import text_format as protobuf_text_format
import logging
import jacp_pb2
import threading
import nacl.utils
import netjoin_session
from netjoin_xiaconf import NetjoinXIAConf

# Build a HandshakeFour protobuf in response to a NetDescriptor beacon
class NetjoinHandshakeFour(object):

    def __init__(self, session, deny=False, client_session=None):
        self.session = session
        self.conf = NetjoinXIAConf()

        # A new HandshakeFour protocol buffer
        self.handshake_four = jacp_pb2.HandshakeFour()
        suite = jacp_pb2.ClientHello.NACL_curve25519xsalsa20poly1305
        self.handshake_four.cyphertext.cipher_suite = suite

        # A container for the encrypted data
        self.cyphertext = jacp_pb2.HandshakeFourProtected()

        # If a client_session was not provided,
        # this message just came over the wire
        # Call from_wire_handshake_four to complete initialization
        if not client_session:
            logging.info("Got handshake 4 over the wire")
            return

        # Put in the plaintext client_session_id
        self.handshake_four.client_session_id = client_session

        # The protobuf defining the encrypted message
        l3_reply = self.cyphertext.gateway_l3_ack_nack
        if deny:
            l3_reply.nack.SetInParent()
        else:
            l3_reply.ack.SetInParent()
        self.cyphertext.client_session_id = client_session

    def get_client_session_id(self):
        return self.cyphertext.client_session_id

    # MUST call every time before sending out handshake three
    def update_nonce(self):
        # Create a new nonce
        nonce = self.session.auth.get_nonce()

        # Encrypt the message with newly created nonce
        self.handshake_four.cyphertext.nonce = nonce
        data_to_encrypt = self.cyphertext.SerializeToString()
        self.handshake_four.cyphertext.cyphertext = self.session.auth.encrypt(data_to_encrypt, nonce)

    def layer_three_acked(self):
        response_t = self.cyphertext.gateway_l3_ack_nack.WhichOneof("l3_reply")
        if response_t == "nack":
            return False
        return True

    def replies_acked(self):
        if not self.layer_three_acked():
            logging.info("Layer 3 request denied")
            return False
        else:
            logging.info("Valid handshake four received")
            return True

    def valid_client_session_id(self):
        plain_client_sess_id = self.handshake_four.client_session_id
        secure_client_sess_id = self.cyphertext.client_session_id
        return plain_client_sess_id == secure_client_sess_id

    def handshake_four_str(self):
        return protobuf_text_format.MessageToString(self.handshake_four)

    def print_handshake_four(self):
        print self.handshake_four_str()

    def cyphertext_str(self):
        return protobuf_text_format.MessageToString(self.cyphertext)

    def print_cyphertext(self):
        print self.cyphertext_str()

    # wire_handshake_four is actually a serialized jacp_pb2.HandshakeFour
    def from_wire_handshake_four(self, wire_handshake_four):

        # Populate the internal handshake one protobuf
        self.handshake_four.CopyFrom(wire_handshake_four)

        # Decrypt cyphertext and make it available
        encrypted_cyphertext = self.handshake_four.cyphertext.cyphertext
        serialized_cyphertext = self.session.auth.decrypt(encrypted_cyphertext)

        # Populate the internal cyphertext after decrypting it
        self.cyphertext.ParseFromString(serialized_cyphertext)

if __name__ == "__main__":
    shutdown_event = threading.Event()
    session = netjoin_session.NetjoinSession(shutdown_event)
    session.daemon = True
    session.start()
    # Hack: setting their verify key same as ours
    session.auth.set_their_raw_verify_key(session.auth.get_raw_verify_key())
    mac = nacl.utils.random(6)
    handshake_four = NetjoinHandshakeFour(session, mac)
    handshake_four.update_nonce()
    serialized_handshake_four = handshake_four.handshake_four.SerializeToString()
    size = len(serialized_handshake_four)
    logging.debug("Serialized handshake one size: {}".format(size))
    handshake_four.print_handshake_four()
    shutdown_event.set()
    print "PASSED: handshake one test"
