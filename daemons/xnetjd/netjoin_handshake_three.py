#!/usr/bin/python
#

from google.protobuf import text_format as protobuf_text_format
import logging
import jacp_pb2
import threading
import nacl.utils
import netjoin_session
from netjoin_xiaconf import NetjoinXIAConf

# Build a HandshakeThree protobuf in response to a NetDescriptor beacon
class NetjoinHandshakeThree(object):

    def __init__(self, session, deny=False, challenge=None,
            gateway_session=None):
        self.session = session
        self.conf = NetjoinXIAConf()

        # A new HandshakeThree protocol buffer
        self.handshake_three = jacp_pb2.HandshakeThree()
        suite = jacp_pb2.ClientHello.NACL_curve25519xsalsa20poly1305
        self.handshake_three.cyphertext.cipher_suite = suite

        # A container for the encrypted data
        self.cyphertext = jacp_pb2.HandshakeThreeProtected()

        # If a challenge was not provided, this message just came over the wire
        # Call from_wire_handshake_three to complete initialization
        if not challenge:
            return

        if not gateway_session:
            logging.error("Gateway session ID not provided during H3 creation")
            return

        # A new nonce for the outgoing handshake three
        self.handshake_three.cyphertext.nonce = session.auth.get_nonce()

        # Put in the plaintext gateway_session_id
        self.handshake_three.gateway_session_id = gateway_session

        # The protobuf defining the encrypted message
        l2_reply = self.cyphertext.client_l2_ack_nack
        l3_reply = self.cyphertext.client_l3_ack_nack
        gc_reply = self.cyphertext.gateway_gc_reply
        if deny:
            l2_reply.nack.SetInParent()
            l3_reply.nack.SetInParent()
            gc_reply.deny.SetInParent()
        else:
            l2_reply.ack.SetInParent()
            l3_reply.ack.SetInParent()
            gc_reply.accept.SetInParent()
        self.cyphertext.gateway_session_id = gateway_session

        # Encrypt the message with nonce received in handshake one (challenge)
        data_to_encrypt = self.cyphertext.SerializeToString()
        self.handshake_three.cyphertext.cyphertext = session.auth.encrypt(data_to_encrypt, challenge)

    def layer_three_granted(self):
        l2_response_t = self.cyphertext.client_l2_ack_nack.WhichOneof("l2_reply")
        if l2_response_t == "nack":
            return False
        return True

    def layer_three_granted(self):
        l3_response_t = self.cyphertext.client_l3_ack_nack.WhichOneof("l3_reply")
        if l3_response_t == "nack":
            return False
        return True

    def gateway_creds_granted(self):
        gc_response_t = self.cyphertext.gateway_gc_reply.WhichOneof("client_response")
        if gc_response_t == "deny":
            return False
        return True

    def join_granted(self):
        if not self.layer_three_granted():
            logging.info("Layer 2 request denied")
            return False
        elif not self.layer_three_granted():
            logging.info("Layer 3 request denied")
            return False
        elif not self.gateway_creds_granted():
            logging.info("Client credentials invalid")
            return False
        else:
            logging.info("Valid handshake three received")
            return True

    def valid_gateway_session_id(self):
        plain_gateway_sess_id = self.handshake_three.gateway_session_id
        secure_gateway_sess_id = self.cyphertext.gateway_session_id
        return plain_gateway_sess_id == secure_gateway_sess_id

    def handshake_three_str(self):
        return protobuf_text_format.MessageToString(self.handshake_three)

    def print_handshake_three(self):
        print self.handshake_three_str()

    def cyphertext_str(self):
        return protobuf_text_format.MessageToString(self.cyphertext)

    def print_cyphertext(self):
        print self.cyphertext_str()

    # wire_handshake_three is actually a serialized jacp_pb2.HandshakeThree
    def from_wire_handshake_three(self, wire_handshake_three):

        # Populate the internal handshake one protobuf
        self.handshake_three.CopyFrom(wire_handshake_three)

        # Decrypt cyphertext and make it available
        encrypted_cyphertext = self.handshake_three.cyphertext.cyphertext
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
    handshake_three = NetjoinHandshakeThree(session, mac)
    handshake_three.update_nonce()
    serialized_handshake_three = handshake_three.handshake_three.SerializeToString()
    size = len(serialized_handshake_three)
    logging.debug("Serialized handshake one size: {}".format(size))
    handshake_three.print_handshake_three()
    shutdown_event.set()
    print "PASSED: handshake one test"
