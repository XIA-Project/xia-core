#!/usr/bin/python
#

import time
import socket
import struct
import logging
import ndap_pb2
import threading
import Queue
from netjoin_beacon import NetjoinBeacon
from netjoin_message_pb2 import NetjoinMessage
from netjoin_authsession import NetjoinAuthsession
from netjoin_handshake_one import NetjoinHandshakeOne

# A client session reperesenting a client joining a network
class NetjoinSession(threading.Thread):
    next_session_ID = 1

    def __init__(self, shutdown_event, auth=None):
        threading.Thread.__init__(self)
        (self.START, self.SEND_HS_ONE, self.VALIDATE_HS_ONE) = range(3)

        self.xianetjoin = ("127.0.0.1", 9882)
        self.beacon = NetjoinBeacon()
        self.state = self.START
        self.shutdown_event = shutdown_event
        self.auth = auth
        self.session_ID = NetjoinSession.next_session_ID
        NetjoinSession.next_session_ID += 1

        # Create an encryption/authentication session if one not provided
        if not self.auth:
            self.auth = NetjoinAuthsession()

        # A queue for receiving NetjoinMessage messages
        self.q = Queue.Queue()
        self.q.timeout = 0.1

        # A socket for sending messages to XIANetJoin
        self.sockfd = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    def get_ID(self):
        return self.session_ID

    # Push a NetjoinMessage to the processing queue from another thread
    def push(self, message_tuple):
        self.q.put(message_tuple)

    # Send a message out to a specific recipient
    def send_netjoin_message(self, message, interface, theirmac):
        netj_header = struct.pack("H6B", interface, theirmac[0], theirmac[1],
                theirmac[2], theirmac[3], theirmac[4], theirmac[5])
        outgoing_packet = netj_header + message.SerializeToString()
        self.sockfd.sendto(outgoing_packet, self.xianetjoin)

    def send_handshake_one(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        # Save access point verify key included in NetDescriptor message
        join_auth_info = message.net_descriptor.ac_shared.ja
        gateway_raw_key = join_auth_info.gateway_ephemeral_pubkey.the_key
        self.auth.set_their_raw_verify_key(gateway_raw_key)

        # Build handshake one
        netjoin_h1 = NetjoinHandshakeOne(self, mymac)
        netjoin_h1.update_nonce()

        outgoing_message = NetjoinMessage()
        outgoing_message.handshake_one.CopyFrom(netjoin_h1.handshake_one)

        # Send handshake one
        self.send_netjoin_message(outgoing_message, interface, theirmac)

    def handle_handshake_one(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        netjoin_h1 = NetjoinHandshakeOne(self, mymac)
        netjoin_h1.from_wire_handshake_one(message.handshake_one)
        netjoin_h1.print_handshake_one()
        netjoin_h1.print_payload()

        if netjoin_h1.is_valid():
            logging.info("Accepted handshake one from client")
        logging.info("Now send handshake two")
        # TODO: Build and send handshake two

    # Main thread handles all messages based on state of joining session
    def run(self):
        logging.debug("Started session ID: {}".format(self.session_ID))

        while not self.shutdown_event.is_set():

            # Keep waiting for a message indefinitely
            try:
                message_tuple = self.q.get(block=True, timeout=self.q.timeout)
                message = message_tuple[0]
            except (Queue.Empty):
                continue
            # We got a message, determine if we are waiting for it
            message_type = message.WhichOneof("message_type")
            if self.state == self.START:
                if message_type == "net_descriptor":
                    self.state = self.SEND_HS_ONE
                    self.send_handshake_one(message_tuple)
                elif message_type == "handshake_one":
                    self.state = self.VALIDATE_HS_ONE
                    self.handle_handshake_one(message_tuple)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    break

        logging.debug("Shutting down session ID: {}".format(self.session_ID))


# Parse arguments and launch necessary threads
def main():

    # Setup logging for this application
    logging.basicConfig(level=logging.DEBUG, format='%(asctime)s: %(module)s %(levelname)s: %(message)s')

    # Set default parameters for an announcer
    net_descriptor = ndap_pb2.NetDescriptor()
    shutdown_event = threading.Event()

    # Create a NetjoinSession
    session1 = NetjoinSession(net_descriptor, shutdown_event)
    session2 = NetjoinSession(net_descriptor, shutdown_event)
    session1.start()
    session2.start()

    # Wait for a while
    time.sleep(10)

    # Now trigger the shutdown_event
    shutdown_event.set()

if __name__ == "__main__":
    main()
