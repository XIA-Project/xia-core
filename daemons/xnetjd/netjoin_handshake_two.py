#!/usr/bin/python
#

from google.protobuf import text_format as protobuf_text_format
import logging
import jacp_pb2
import threading
import nacl.utils
import netjoin_session
from netjoin_xiaconf import NetjoinXIAConf

# Build a HandshakeTwo protobuf in response to a NetDescriptor beacon
class NetjoinHandshakeTwo(object):

    def __init__(self, session, deny=False, challenge=None,
            client_session=None):
        self.session = session
        self.conf = NetjoinXIAConf()
        self.challenge = challenge

        # A new HandshakeTwo protocol buffer
        self.handshake_two = jacp_pb2.HandshakeTwo()
        suite = jacp_pb2.ClientHello.NACL_curve25519xsalsa20poly1305
        self.handshake_two.cyphertext.cipher_suite = suite

        # A container for the encrypted data
        self.cyphertext = jacp_pb2.HandshakeTwoProtected()

        # If a challenge was not provided, this message just came over the wire
        # Call from_wire_handshake_two to complete initialization
        if not challenge:
            return

        if not client_session:
            logging.error("Client session ID not provided during H2 creation")
            return

        # A new nonce for the outgoing handshake two
        self.handshake_two.cyphertext.nonce = session.auth.get_nonce()

        # Put in the plaintext client_session_id
        self.handshake_two.cyphertext.client_session_id = client_session

        # The protobuf defining the encrypted message
        l2_reply = self.cyphertext.gateway_l2_reply
        l3_reply = self.cyphertext.gateway_l3_reply
        cc_reply = self.cyphertext.gateway_cc_reply
        if deny:
            l2_reply.deny.SetInParent()
            l3_reply.deny.SetInParent()
            cc_reply.deny.SetInParent()
        else:
            l2_reply.grant.ethernet.SetInParent()
            # TODO: Add XHCP beacon info in pxhcp below
            xhcp_reply = l3_reply.grant.XIP.single.pxhcp
            xhcp_reply.router_dag = self.conf.get_router_dag()
            xhcp_reply.nameserver_dag = self.conf.get_ns_dag()
            cc_reply.accept.SetInParent()
        self.cyphertext.gateway_credentials.SetInParent()
        self.cyphertext.client_session_id = client_session
        self.cyphertext.gateway_session_id = self.session.get_ID()

        # Encrypt the message with nonce received in handshake one (challenge)
        data_to_encrypt = self.cyphertext.SerializeToString()
        self.handshake_two.cyphertext.cyphertext = session.auth.encrypt(data_to_encrypt, challenge)

    def layer_two_granted(self):
        l2_response_t = self.cyphertext.gateway_l2_reply.WhichOneof("l2_reply")
        if l2_response_t == "deny":
            return False
        return True

    def layer_three_granted(self):
        l3_response_t = self.cyphertext.gateway_l3_reply.WhichOneof("l3_reply")
        if l3_response_t == "deny":
            return False
        return True

    def client_creds_granted(self):
        cc_response_t = self.cyphertext.gateway_cc_reply.WhichOneof("gateway_response")
        if cc_response_t == "deny":
            return False
        return True

    def join_granted(self):
        if not self.layer_two_granted():
            logging.info("Layer 2 request denied")
            return False
        elif not self.layer_three_granted():
            logging.info("Layer 3 request denied")
            return False
        elif not self.client_creds_granted():
            logging.info("Client credentials invalid")
            return False
        else:
            logging.info("Valid handshake two received")
            return True

    def valid_client_session_id(self):
        plain_client_sess_id = self.handshake_two.cyphertext.client_session_id
        secure_client_sess_id = self.cyphertext.client_session_id
        return plain_client_sess_id == secure_client_sess_id

    def handshake_two_str(self):
        return protobuf_text_format.MessageToString(self.handshake_two)

    def print_handshake_two(self):
        print self.handshake_two_str()

    def cyphertext_str(self):
        return protobuf_text_format.MessageToString(self.cyphertext)

    def print_cyphertext(self):
        print self.cyphertext_str()

    # wire_handshake_two is actually a serialized jacp_pb2.HandshakeTwo
    def from_wire_handshake_two(self, wire_handshake_two):

        # Populate the internal handshake one protobuf
        self.handshake_two.CopyFrom(wire_handshake_two)

        # Decrypt cyphertext and make it available
        encrypted_cyphertext = self.handshake_two.cyphertext.cyphertext
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
    handshake_two = NetjoinHandshakeTwo(session, mac)
    handshake_two.update_nonce()
    serialized_handshake_two = handshake_two.handshake_two.SerializeToString()
    size = len(serialized_handshake_two)
    logging.debug("Serialized handshake one size: {}".format(size))
    handshake_two.print_handshake_two()
    shutdown_event.set()
    print "PASSED: handshake one test"
