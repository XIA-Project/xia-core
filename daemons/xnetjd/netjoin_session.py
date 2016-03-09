#!/usr/bin/python
#

import time
import socket
import struct
import logging
import ndap_pb2
import threading
import Queue
import c_xsocket
from netjoin_beacon import NetjoinBeacon
from netjoin_message_pb2 import NetjoinMessage
from netjoin_authsession import NetjoinAuthsession
from netjoin_handshake_one import NetjoinHandshakeOne
from netjoin_handshake_two import NetjoinHandshakeTwo

# A client session reperesenting a client joining a network
class NetjoinSession(threading.Thread):
    next_session_ID = 1

    def __init__(self, shutdown_event, auth=None):
        threading.Thread.__init__(self)
        (self.START, self.HS_2_WAIT, self.HS_3_WAIT) = range(3)

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

    def handle_net_descriptor(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        logging.info("Got a beacon from a network we want to join")
        # Save access point verify key included in NetDescriptor message
        join_auth_info = message.net_descriptor.ac_shared.ja
        gateway_raw_key = join_auth_info.gateway_ephemeral_pubkey.the_key
        self.auth.set_their_raw_verify_key(gateway_raw_key)

        # Retrieve gateway_nonce as the challenge to include in l3 request
        gw_nonce = join_auth_info.gateway_nonce

        # Build handshake one
        netjoin_h1 = NetjoinHandshakeOne(self, mymac, challenge=gw_nonce)
        netjoin_h1.update_nonce()

        outgoing_message = NetjoinMessage()
        outgoing_message.handshake_one.CopyFrom(netjoin_h1.handshake_one)

        # Send handshake one
        self.send_netjoin_message(outgoing_message, interface, theirmac)

        # Now we will wait for handshake two
        self.state = self.HS_2_WAIT

    def handle_handshake_one(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        # Convert incoming protobuf to a handshake one object we can assess
        netjoin_h1 = NetjoinHandshakeOne(self, mymac)
        netjoin_h1.from_wire_handshake_one(message.handshake_one)
        netjoin_h1.print_handshake_one()
        netjoin_h1.print_payload()

        # Validate incoming client joining request (handshake one)
        deny_h2 = False
        if not netjoin_h1.is_valid():
            deny_h2 = True
            logging.info("HandshakeOne invalid, denying connection request")
        logging.info("Accepted handshake one from client")

        # Retrieve handshake one info to be included in handshake two
        h1_nonce = netjoin_h1.get_nonce()
        client_session_id = netjoin_h1.client_session_id()

        # Now build a handshake two message
        logging.info("Now sending handshake two")
        netjoin_h2 = NetjoinHandshakeTwo(self, deny=deny_h2,
                challenge=h1_nonce, client_session=client_session_id)

        # Package the handshake two into a netjoin message wrapper
        outgoing_message = NetjoinMessage()
        outgoing_message.handshake_two.CopyFrom(netjoin_h2.handshake_two)
        self.send_netjoin_message(outgoing_message, interface, theirmac)

        # Now we will wait for handshake 3
        self.state = self.HS_3_WAIT

    def handle_handshake_two(self, message_tuple):
        message, interface, mymac, theirmac = message_tuple

        logging.info("Got a handshake two message")
        netjoin_h2 = NetjoinHandshakeTwo(self)
        netjoin_h2.from_wire_handshake_two(message.handshake_two)
        netjoin_h2.print_handshake_two()
        netjoin_h2.print_cyphertext()

        # Verify the client_session_id matches the encrypted one
        if not netjoin_h2.valid_client_session_id():
            logging.error("Invalid client session ID")
            return

        # Ensure that our netjoin request was granted
        if not netjoin_h2.join_granted():
            logging.error("Our join request was denied")
            return
        logging.info("Valid handshake two: We can join this network now")

        # Configure Click info
        router_dag = netjoin_h2.router_dag()
        logging.info("Router DAG is: {}".format(router_dag))
        sockfd = c_xsocket.Xsocket(c_xsocket.SOCK_STREAM))
        retval = c_xsocket.XupdateDAG(sockfd, interface, router_dag, "")
        if retval != 0:
            logging.error("Failed updating DAG in XIA stack")
        logging.info("Local DAG updated")
        # TODO: Setup routes to the router

    # Main thread handles all messages based on state of joining session
    def run(self):
        logging.debug("Started session ID: {}".format(self.session_ID))

        while not self.shutdown_event.is_set():

            # Keep waiting for a message indefinitely
            try:
                message_tuple = self.q.get(block=True, timeout=self.q.timeout)
                message = message_tuple[0]
            except (Queue.Empty):
                # TODO: Retransmit last message if not in START state
                continue

            # We got a message, determine if we are waiting for it
            message_type = message.WhichOneof("message_type")
            logging.info("Got a {} message".format(message_type))
            if self.state == self.START:
                if message_type == "net_descriptor":
                    self.handle_net_descriptor(message_tuple)
                elif message_type == "handshake_one":
                    self.handle_handshake_one(message_tuple)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected net_descriptor or handshake one")
                continue

            if self.state == self.HS_2_WAIT:
                if message_type == "handshake_two":
                    self.handle_handshake_two(message_tuple)
                else:
                    logging.error("Invalid message: {}".format(message_type))
                    logging.error("Expected handshake two.")
                continue

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
